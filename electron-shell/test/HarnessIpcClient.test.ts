import assert from 'node:assert/strict';
import { PassThrough } from 'node:stream';
import test from 'node:test';

import { encodeFrame, FrameDecoder } from '../src/ipc/FrameCodec.js';
import { HarnessIpcClient, JsonRpcError } from '../src/ipc/HarnessIpcClient.js';

function createHarness() {
  const clientToCore = new PassThrough();
  const coreToClient = new PassThrough();
  const client = new HarnessIpcClient({ stdin: clientToCore, stdout: coreToClient });
  return { client, clientToCore, coreToClient };
}

function nextMessage(stream: PassThrough): Promise<Record<string, unknown>> {
  const decoder = new FrameDecoder();
  return new Promise((resolve, reject) => {
    const onData = (chunk: Buffer) => {
      try {
        const [body] = decoder.feed(chunk);
        if (body !== undefined) {
          stream.off('data', onData);
          resolve(JSON.parse(body) as Record<string, unknown>);
        }
      } catch (error) {
        reject(error);
      }
    };
    stream.on('data', onData);
  });
}

test('correlates outgoing requests with responses', async () => {
  const { client, clientToCore, coreToClient } = createHarness();
  const outgoing = nextMessage(clientToCore);
  const resultPromise = client.request<{ protocolVersion: string }>('initialize', {
    protocolVersion: '1.0',
  });
  const request = await outgoing;

  assert.equal(request.method, 'initialize');
  coreToClient.write(
    encodeFrame(JSON.stringify({ jsonrpc: '2.0', id: request.id, result: { protocolVersion: '1.0' } })),
  );
  assert.deepEqual(await resultPromise, { protocolVersion: '1.0' });
  client.close();
});

test('turns JSON-RPC error responses into typed errors', async () => {
  const { client, clientToCore, coreToClient } = createHarness();
  const outgoing = nextMessage(clientToCore);
  const resultPromise = client.request('missing/method');
  const request = await outgoing;
  coreToClient.write(
    encodeFrame(
      JSON.stringify({
        jsonrpc: '2.0',
        id: request.id,
        error: { code: -32601, message: 'Method not found' },
      }),
    ),
  );

  await assert.rejects(resultPromise, (error: unknown) => {
    return error instanceof JsonRpcError && error.code === -32601;
  });
  client.close();
});

test('handles reverse permission requests from Core', async () => {
  const { client, clientToCore, coreToClient } = createHarness();
  client.handleRequest('permission/ask', (params) => {
    assert.deepEqual(params, { toolName: 'unreal.actor.spawn' });
    return { decision: 'allow_once' };
  });

  const response = nextMessage(clientToCore);
  coreToClient.write(
    encodeFrame(
      JSON.stringify({
        jsonrpc: '2.0',
        id: 'permission-1',
        method: 'permission/ask',
        params: { toolName: 'unreal.actor.spawn' },
      }),
    ),
  );

  assert.deepEqual(await response, {
    jsonrpc: '2.0',
    id: 'permission-1',
    result: { decision: 'allow_once' },
  });
  client.close();
});
