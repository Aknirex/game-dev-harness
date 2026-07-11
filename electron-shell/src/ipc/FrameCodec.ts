const HEADER_SEPARATOR = Buffer.from('\r\n\r\n', 'ascii');
const CONTENT_TYPE = 'application/vnd.harness+json; charset=utf-8';
const MAX_HEADER_SIZE = 8 * 1024;
const DEFAULT_MAX_BODY_SIZE = 16 * 1024 * 1024;

export class FrameProtocolError extends Error {
  override readonly name = 'FrameProtocolError';
}

export function encodeFrame(body: string | Uint8Array): Buffer {
  const bodyBuffer = typeof body === 'string' ? Buffer.from(body, 'utf8') : Buffer.from(body);
  const header = Buffer.from(
    `Content-Length: ${bodyBuffer.byteLength}\r\nContent-Type: ${CONTENT_TYPE}\r\n\r\n`,
    'ascii',
  );
  return Buffer.concat([header, bodyBuffer]);
}

export class FrameDecoder {
  private buffer = Buffer.alloc(0);

  constructor(private readonly maxBodySize = DEFAULT_MAX_BODY_SIZE) {
    if (!Number.isSafeInteger(maxBodySize) || maxBodySize <= 0) {
      throw new RangeError('maxBodySize must be a positive safe integer');
    }
  }

  feed(chunk: Uint8Array): string[] {
    if (chunk.byteLength === 0) return [];
    this.buffer = Buffer.concat([this.buffer, chunk]);
    const bodies: string[] = [];

    while (true) {
      const headerEnd = this.buffer.indexOf(HEADER_SEPARATOR);
      if (headerEnd < 0) {
        if (this.buffer.byteLength > MAX_HEADER_SIZE) {
          this.fail('Frame header exceeds size limit');
        }
        return bodies;
      }
      if (headerEnd > MAX_HEADER_SIZE) {
        this.fail('Frame header exceeds size limit');
      }

      const headers = this.parseHeaders(this.buffer.subarray(0, headerEnd).toString('ascii'));
      const frameSize = headerEnd + HEADER_SEPARATOR.byteLength + headers.contentLength;
      if (this.buffer.byteLength < frameSize) return bodies;

      const bodyStart = headerEnd + HEADER_SEPARATOR.byteLength;
      const body = this.buffer.subarray(bodyStart, frameSize).toString('utf8');
      this.buffer = this.buffer.subarray(frameSize);
      bodies.push(body);
    }
  }

  get bufferedBytes(): number {
    return this.buffer.byteLength;
  }

  private parseHeaders(rawHeaders: string): { contentLength: number } {
    let contentLength: number | undefined;
    let hasValidContentType = false;

    for (const line of rawHeaders.split('\r\n')) {
      const colon = line.indexOf(':');
      if (colon <= 0) this.fail('Malformed frame header');
      const name = line.slice(0, colon).trim().toLowerCase();
      const value = line.slice(colon + 1).trim();

      if (name === 'content-length') {
        if (contentLength !== undefined || !/^(0|[1-9]\d*)$/.test(value)) {
          this.fail('Invalid Content-Length header');
        }
        contentLength = Number(value);
        if (!Number.isSafeInteger(contentLength) || contentLength > this.maxBodySize) {
          this.fail('Frame body exceeds size limit');
        }
      } else if (name === 'content-type') {
        hasValidContentType = value.toLowerCase() === CONTENT_TYPE;
      }
    }

    if (contentLength === undefined) this.fail('Missing Content-Length header');
    if (!hasValidContentType) this.fail('Missing or unsupported Content-Type header');
    return { contentLength };
  }

  private fail(message: string): never {
    this.buffer = Buffer.alloc(0);
    throw new FrameProtocolError(message);
  }
}
