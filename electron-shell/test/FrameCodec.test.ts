import assert from 'node:assert/strict';
import test from 'node:test';

import { encodeFrame, FrameDecoder, FrameProtocolError } from '../src/ipc/FrameCodec.js';

test('decodes fragmented UTF-8 frames by byte length', () => {
  const frame = encodeFrame(JSON.stringify({ message: '你好，Harness' }));
  const decoder = new FrameDecoder();
  const bodies: string[] = [];

  for (const byte of frame) bodies.push(...decoder.feed(Buffer.from([byte])));

  assert.deepEqual(bodies.map((body) => JSON.parse(body)), [{ message: '你好，Harness' }]);
  assert.equal(decoder.bufferedBytes, 0);
});

test('decodes multiple frames from one chunk', () => {
  const decoder = new FrameDecoder();
  const frames = Buffer.concat([encodeFrame('{}'), encodeFrame('{"id":2}')]);
  assert.deepEqual(decoder.feed(frames), ['{}', '{"id":2}']);
});

test('rejects missing content type and oversized bodies', () => {
  const missingType = new FrameDecoder();
  assert.throws(
    () => missingType.feed(Buffer.from('Content-Length: 2\r\n\r\n{}')),
    FrameProtocolError,
  );

  const limited = new FrameDecoder(4);
  assert.throws(() => limited.feed(encodeFrame('12345')), FrameProtocolError);
});
