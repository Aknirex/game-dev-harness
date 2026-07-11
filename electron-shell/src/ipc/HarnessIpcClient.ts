import { spawn, type ChildProcessWithoutNullStreams } from 'node:child_process';
import type { Readable, Writable } from 'node:stream';

import { encodeFrame, FrameDecoder } from './FrameCodec.js';

export type JsonRpcId = number | string;
export type JsonRpcParams = Record<string, unknown> | unknown[];

export interface JsonRpcErrorObject {
  code: number;
  message: string;
  data?: unknown;
}

export class JsonRpcError extends Error {
  override readonly name = 'JsonRpcError';

  constructor(
    readonly code: number,
    message: string,
    readonly data?: unknown,
  ) {
    super(message);
  }
}

export interface IpcTransport {
  stdin: Writable;
  stdout: Readable;
  stderr?: Readable;
  kill?: () => boolean;
}

type PendingRequest = {
  resolve: (value: unknown) => void;
  reject: (reason: Error) => void;
  timeout: NodeJS.Timeout;
};
type RequestHandler = (params: JsonRpcParams | undefined) => unknown | Promise<unknown>;
type NotificationHandler = (params: JsonRpcParams | undefined) => void;

const INTERNAL_ERROR = -32603;
const METHOD_NOT_FOUND = -32601;

export class HarnessIpcClient {
  private readonly decoder = new FrameDecoder();
  private readonly pendingRequests = new Map<JsonRpcId, PendingRequest>();
  private readonly requestHandlers = new Map<string, RequestHandler>();
  private readonly notificationHandlers = new Map<string, Set<NotificationHandler>>();
  private nextRequestId = 1;
  private closed = false;

  constructor(private readonly transport: IpcTransport) {
    transport.stdout.on('data', (chunk: Buffer) => this.handleData(chunk));
    transport.stdout.on('end', () => this.close(new Error('Harness Core stdout closed')));
    transport.stdout.on('error', (error) => this.close(error));
  }

  static spawn(corePath: string, args: readonly string[] = []): HarnessIpcClient {
    const process: ChildProcessWithoutNullStreams = spawn(corePath, args, {
      stdio: ['pipe', 'pipe', 'pipe'],
      windowsHide: true,
    });
    return new HarnessIpcClient(process);
  }

  async request<T = unknown>(
    method: string,
    params: JsonRpcParams = {},
    timeoutMs = 30_000,
  ): Promise<T> {
    if (this.closed) throw new Error('IPC client is closed');
    if (!method) throw new TypeError('method must not be empty');
    if (!Number.isSafeInteger(timeoutMs) || timeoutMs <= 0) {
      throw new RangeError('timeoutMs must be a positive safe integer');
    }

    const id = this.nextRequestId++;
    const promise = new Promise<unknown>((resolve, reject) => {
      const timeout = setTimeout(() => {
        this.pendingRequests.delete(id);
        reject(new Error(`JSON-RPC request timed out: ${method}`));
      }, timeoutMs);
      this.pendingRequests.set(id, { resolve, reject, timeout });
    });
    this.write({ jsonrpc: '2.0', id, method, params });
    return promise as Promise<T>;
  }

  notify(method: string, params: JsonRpcParams = {}): void {
    if (this.closed) throw new Error('IPC client is closed');
    if (!method) throw new TypeError('method must not be empty');
    this.write({ jsonrpc: '2.0', method, params });
  }

  onNotification(method: string, handler: NotificationHandler): () => void {
    const handlers = this.notificationHandlers.get(method) ?? new Set<NotificationHandler>();
    handlers.add(handler);
    this.notificationHandlers.set(method, handlers);
    return () => handlers.delete(handler);
  }

  handleRequest(method: string, handler: RequestHandler): () => void {
    if (this.requestHandlers.has(method)) {
      throw new Error(`A request handler is already registered for ${method}`);
    }
    this.requestHandlers.set(method, handler);
    return () => this.requestHandlers.delete(method);
  }

  async shutdown(): Promise<void> {
    if (this.closed) return;
    await this.request('shutdown');
    this.notify('exit');
    this.close(new Error('IPC client shut down'));
  }

  close(reason = new Error('IPC client closed')): void {
    if (this.closed) return;
    this.closed = true;
    for (const pending of this.pendingRequests.values()) {
      clearTimeout(pending.timeout);
      pending.reject(reason);
    }
    this.pendingRequests.clear();
    this.transport.stdin.end();
  }

  private handleData(chunk: Buffer): void {
    try {
      for (const body of this.decoder.feed(chunk)) {
        void this.handleMessage(JSON.parse(body) as unknown).catch((error: unknown) => {
          this.close(error instanceof Error ? error : new Error(String(error)));
        });
      }
    } catch (error) {
      this.close(error instanceof Error ? error : new Error(String(error)));
    }
  }

  private async handleMessage(message: unknown): Promise<void> {
    if (!isRecord(message) || message.jsonrpc !== '2.0') {
      throw new Error('Received an invalid JSON-RPC message');
    }

    if (typeof message.method === 'string') {
      const params = isParams(message.params) ? message.params : undefined;
      if (isId(message.id)) {
        await this.handleReverseRequest(message.id, message.method, params);
      } else {
        for (const handler of this.notificationHandlers.get(message.method) ?? []) {
          handler(params);
        }
      }
      return;
    }

    if (!isId(message.id)) throw new Error('JSON-RPC response has an invalid id');
    const pending = this.pendingRequests.get(message.id);
    if (!pending) return;
    this.pendingRequests.delete(message.id);
    clearTimeout(pending.timeout);

    if (isErrorObject(message.error)) {
      pending.reject(new JsonRpcError(message.error.code, message.error.message, message.error.data));
    } else if ('result' in message) {
      pending.resolve(message.result);
    } else {
      pending.reject(new Error('JSON-RPC response has neither result nor error'));
    }
  }

  private async handleReverseRequest(
    id: JsonRpcId,
    method: string,
    params: JsonRpcParams | undefined,
  ): Promise<void> {
    const handler = this.requestHandlers.get(method);
    if (!handler) {
      this.writeError(id, METHOD_NOT_FOUND, 'Method not found');
      return;
    }
    try {
      const result = await handler(params);
      this.write({ jsonrpc: '2.0', id, result: result ?? null });
    } catch (error) {
      this.writeError(id, INTERNAL_ERROR, error instanceof Error ? error.message : 'Internal error');
    }
  }

  private writeError(id: JsonRpcId, code: number, message: string): void {
    this.write({ jsonrpc: '2.0', id, error: { code, message } });
  }

  private write(message: object): void {
    this.transport.stdin.write(encodeFrame(JSON.stringify(message)));
  }
}

function isRecord(value: unknown): value is Record<string, unknown> {
  return typeof value === 'object' && value !== null && !Array.isArray(value);
}

function isId(value: unknown): value is JsonRpcId {
  return typeof value === 'string' || (typeof value === 'number' && Number.isSafeInteger(value));
}

function isParams(value: unknown): value is JsonRpcParams {
  return Array.isArray(value) || isRecord(value);
}

function isErrorObject(value: unknown): value is JsonRpcErrorObject {
  return (
    isRecord(value) &&
    typeof value.code === 'number' &&
    Number.isInteger(value.code) &&
    typeof value.message === 'string'
  );
}
