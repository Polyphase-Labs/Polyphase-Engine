#!/usr/bin/env node
/**
 * Polyphase WebSocket echo server - the test bed for Engine/Source/Network/
 * WebSocket*. Dependency-free (node >= 16, http + crypto only), so it runs from
 * a checkout with no npm install.
 *
 * Behaviour, matching WsEchoTest.lua:
 *   - echoes every text and binary message back byte-identically
 *   - answers Ping with Pong
 *   - accepts the subprotocol "echo.v1" and echoes the negotiated value back in
 *     the handshake
 *   - on the text message "close-me", initiates a server Close(4000, "requested")
 *
 * The handshake and framing are adapted from the WebGL2 build target's
 * Tools/relay/relay.js, which is RFC-vector verified.
 *
 * Usage:
 *   node echo.js [--host 127.0.0.1] [--port 9002] [-v]
 */

'use strict';

const http   = require('node:http');
const crypto = require('node:crypto');

// ---------------------------------------------------------------------------
// Args
// ---------------------------------------------------------------------------

const args = process.argv.slice(2);
let host = '127.0.0.1';
let port = 9002;
let verbose = false;

for (let i = 0; i < args.length; ++i) {
    const a = args[i];
    if (a === '--host') host = args[++i];
    else if (a === '--port') port = parseInt(args[++i], 10);
    else if (a === '-v' || a === '--verbose') verbose = true;
    else if (a === '-h' || a === '--help') {
        console.log('usage: node echo.js [--host H] [--port P] [-v]');
        process.exit(0);
    } else {
        console.error('unknown argument: ' + a);
        process.exit(1);
    }
}

// ---------------------------------------------------------------------------
// Minimal WebSocket server (RFC 6455)
// ---------------------------------------------------------------------------

// RFC 6455 handshake GUID. Verified against the spec's test vector:
// "dGhlIHNhbXBsZSBub25jZQ==" -> "s3pPLMBiTxaQ9kYGzzhZRbK+xOo="
const WS_GUID = '258EAFA5-E914-47DA-95CA-C5AB0DC85B11';

const OP_CONT = 0x0;
const OP_TEXT = 0x1;
const OP_BIN  = 0x2;
const OP_CLOSE = 0x8;
const OP_PING = 0x9;
const OP_PONG = 0xA;

const SUPPORTED_PROTOCOL = 'echo.v1';

function wsAccept(key) {
    return crypto.createHash('sha1').update(key + WS_GUID).digest('base64');
}

// Server-to-client frames are never masked (RFC 6455 5.1).
function wsEncode(opcode, payload) {
    const len = payload.length;
    let header;

    if (len < 126) {
        header = Buffer.alloc(2);
        header[1] = len;
    } else if (len < 65536) {
        header = Buffer.alloc(4);
        header[1] = 126;
        header.writeUInt16BE(len, 2);
    } else {
        header = Buffer.alloc(10);
        header[1] = 127;
        header.writeBigUInt64BE(BigInt(len), 2);
    }

    header[0] = 0x80 | opcode;   // FIN | opcode
    return Buffer.concat([header, payload]);
}

// Pulls complete frames out of an accumulating buffer. Returns bytes consumed,
// or -1 on a protocol error the caller should close over.
function wsDecode(buf, onMessage, onClose, onPing) {
    let off = 0;

    // Fragment reassembly state lives on the function's caller via closure
    // args; this decoder handles one frame at a time and hands continuation
    // frames straight through with their opcode.
    for (;;) {
        if (buf.length - off < 2) break;

        const b0 = buf[off];
        const b1 = buf[off + 1];
        const fin = (b0 & 0x80) !== 0;
        const opcode = b0 & 0x0F;
        const masked = (b1 & 0x80) !== 0;
        let len = b1 & 0x7F;
        let p = off + 2;

        if (len === 126) {
            if (buf.length - p < 2) break;
            len = buf.readUInt16BE(p);
            p += 2;
        } else if (len === 127) {
            if (buf.length - p < 8) break;
            const big = buf.readBigUInt64BE(p);
            if (big > 0x7FFFFFFFn) return -1;
            len = Number(big);
            p += 8;
        }

        // A client MUST mask (RFC 6455 5.3). Refuse anything else - that is
        // exactly the bug this server exists to catch.
        if (!masked) return -1;

        if (buf.length - p < 4) break;
        const maskKey = buf.subarray(p, p + 4);
        p += 4;

        if (buf.length - p < len) break;

        const payload = Buffer.from(buf.subarray(p, p + len));
        for (let i = 0; i < payload.length; ++i) payload[i] ^= maskKey[i & 3];
        p += len;
        off = p;

        if (opcode === OP_CLOSE) { onClose(payload); return -1; }
        else if (opcode === OP_PING) { onPing(payload); }
        else if (opcode === OP_PONG) { /* ignored */ }
        else if (opcode === OP_TEXT || opcode === OP_BIN || opcode === OP_CONT) {
            onMessage(opcode, fin, payload);
        }
        else return -1;
    }

    return off;
}

// ---------------------------------------------------------------------------
// Connection handling
// ---------------------------------------------------------------------------

let connSeq = 0;

function handleConnection(sock) {
    const id = ++connSeq;
    let closed = false;
    let acc = Buffer.alloc(0);

    let fragments = [];
    let fragmentOpcode = 0;

    const log = (...m) => { if (verbose) console.log(`[conn ${id}]`, ...m); };

    function send(opcode, payload) {
        if (!closed && sock.writable) sock.write(wsEncode(opcode, payload));
    }

    function sendClose(code, reason) {
        const body = Buffer.alloc(2 + Buffer.byteLength(reason));
        body.writeUInt16BE(code, 0);
        body.write(reason, 2);
        send(OP_CLOSE, body);
    }

    function cleanup() {
        if (closed) return;
        closed = true;
        try { sock.destroy(); } catch (e) {}
        log('closed');
    }

    function onCompleteMessage(opcode, payload) {
        const isBinary = opcode === OP_BIN;

        if (!isBinary && payload.toString('utf8') === 'close-me') {
            log('client asked for a server-initiated close');
            sendClose(4000, 'requested');
            // Give the frame a moment to flush, then drop the socket.
            setTimeout(cleanup, 250);
            return;
        }

        log(`echo ${isBinary ? 'binary' : 'text'} ${payload.length} bytes`);
        send(opcode, payload);
    }

    sock.on('data', (chunk) => {
        acc = acc.length ? Buffer.concat([acc, chunk]) : chunk;

        const consumed = wsDecode(
            acc,
            (opcode, fin, payload) => {
                if (opcode === OP_CONT) {
                    fragments.push(payload);
                } else if (!fin) {
                    fragmentOpcode = opcode;
                    fragments = [payload];
                } else {
                    onCompleteMessage(opcode, payload);
                    return;
                }

                if (fin) {
                    onCompleteMessage(fragmentOpcode, Buffer.concat(fragments));
                    fragments = [];
                }
            },
            (payload) => {
                // Echo the close back so the client sees a clean handshake.
                const code = payload.length >= 2 ? payload.readUInt16BE(0) : 1000;
                const reason = payload.length > 2 ? payload.subarray(2).toString('utf8') : '';
                log('client close', code, JSON.stringify(reason));
                sendClose(code, reason);
                setTimeout(cleanup, 100);
            },
            (payload) => send(OP_PONG, payload)
        );

        if (consumed < 0) { cleanup(); return; }
        acc = consumed > 0 ? acc.subarray(consumed) : acc;
    });

    sock.on('close', cleanup);
    sock.on('error', cleanup);

    log('open');
}

// ---------------------------------------------------------------------------
// Server
// ---------------------------------------------------------------------------

const server = http.createServer((req, res) => {
    res.writeHead(200, { 'Content-Type': 'text/plain' });
    res.end('Polyphase WebSocket echo server. Connect a WebSocket to this address.\n');
});

server.on('upgrade', (req, sock, head) => {
    const key = req.headers['sec-websocket-key'];
    if (req.headers.upgrade !== 'websocket' || !key) {
        sock.destroy();
        return;
    }

    let selected = null;
    const offered = req.headers['sec-websocket-protocol'];
    if (offered) {
        const list = offered.split(',').map(s => s.trim());
        if (list.includes(SUPPORTED_PROTOCOL)) selected = SUPPORTED_PROTOCOL;
    }

    let response =
        'HTTP/1.1 101 Switching Protocols\r\n' +
        'Upgrade: websocket\r\n' +
        'Connection: Upgrade\r\n' +
        'Sec-WebSocket-Accept: ' + wsAccept(key) + '\r\n';

    if (selected) response += 'Sec-WebSocket-Protocol: ' + selected + '\r\n';
    response += '\r\n';

    sock.write(response);
    sock.setNoDelay(true);
    handleConnection(sock);

    if (head && head.length) sock.unshift(head);
});

server.listen(port, host, () => {
    console.log(`Polyphase WebSocket echo server listening on ws://${host}:${port}`);
    console.log(`Subprotocol offered: ${SUPPORTED_PROTOCOL}`);
    console.log('Send the text "close-me" to have the server close with 4000 "requested".');
});
