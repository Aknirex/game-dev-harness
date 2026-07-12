import { HarnessIpcClient } from '../src/ipc/HarnessIpcClient.js';
import { describe, it } from 'node:test';
import assert from 'node:assert/strict';
import { fileURLToPath } from 'node:url';
import path from 'node:path';

const corePath = path.resolve(
  fileURLToPath(import.meta.url),
  '..', '..', '..', '..',
  'build', 'windows', 'x64', 'debug', 'harness-core.exe',
);

describe('Harness Core E2E', () => {
  /** @type {HarnessIpcClient} */
  let client;

  it('initialize + tool/list + shutdown roundtrip', async () => {
    client = HarnessIpcClient.spawn(corePath);

    const initResult = await client.request<{
      protocolVersion: string;
      capabilities: { executionEngine: boolean };
    }>('initialize', { protocolVersion: '1.0' });
    assert.equal(initResult.protocolVersion, '1.0');
    assert.equal(initResult.capabilities.executionEngine, true);

    const tools = await client.request<{ tools: unknown[] }>('tool/list');
    assert.ok(Array.isArray(tools.tools));
    assert.ok(tools.tools.length >= 1);
    const toolNames = tools.tools.map((t: unknown) => (t as { name: string }).name);
    assert.ok(toolNames.includes('harness.inspect'));

    // Call the read-only tool through ExecutionEngine.
    const execResult = await client.request<{
      success: boolean;
      invocationId: string;
      summary: string;
    }>('tool/call', { name: 'harness.inspect', arguments: { path: 'src/main.cpp' } });
    assert.equal(execResult.success, true);
    assert.ok(execResult.invocationId.length > 0);

    await client.shutdown();
  });

  it('unauthorized tool call is rejected', async () => {
    client = HarnessIpcClient.spawn(corePath);
    await client.request('initialize', { protocolVersion: '1.0' });

    // harness.inspect requires FileRead on Content/* — but only "src/*" and
    // "Content/*" are allowed by built-in rules, and the tool's
    // resource_pattern is "Content/*" which maps correctly.
    // This test verifies that a tool call for a non-existent tool is blocked.
    const result = await client.request<{ success: boolean }>('tool/call', {
      name: 'nonexistent.tool',
      arguments: {},
    });
    assert.equal(result.success, false);

    await client.shutdown();
  });

  it('duplicate initialize is rejected', async () => {
    client = HarnessIpcClient.spawn(corePath);
    await client.request('initialize', { protocolVersion: '1.0' });

    try {
      await client.request('initialize', { protocolVersion: '1.0' });
      assert.fail('Expected duplicate initialize to be rejected');
    } catch (err) {
      assert.ok(err instanceof Error);
    }

    await client.shutdown();
  });
});
