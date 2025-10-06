#!/usr/bin/env python3
import socket, os, argparse, sys, json, random

COAP_VER=1
TYPE_CON=0
TYPE_NON=1
GET,POST,PUT,DELETE=1,2,3,4
OPT_URI_PATH=11

def enc_n(n):
    if n<13: return bytes([n])
    if n<269: return bytes([13, n-13])
    return bytes([14, (n-269)>>8, (n-269)&0xFF])

def add_opt(b, last, num, val:bytes):
    delta = num - last
    db = enc_n(delta); lb = enc_n(len(val))
    b += bytes([(db[0]<<4)|lb[0]]) + db[1:] + lb[1:] + val
    return b, num

def build_msg(_type, code, mid, path, payload=b"", token=None):
    if token is None: token = os.urandom(4)
    tkl=len(token)
    b = bytearray()
    b.append((COAP_VER<<6)|(_type<<4)|tkl)
    b.append(code)
    b += mid.to_bytes(2,'big')
    b += token
    last=0
    for seg in [p for p in path.split('/') if p]:
        b, last = add_opt(b,last,OPT_URI_PATH, seg.encode())
    if payload:
        b.append(0xFF); b += payload
    return bytes(b), token

def parse_reply(buf:bytes):
    if len(buf)<4: return None
    ver=(buf[0]>>6)&3; typ=(buf[0]>>4)&3; tkl=buf[0]&0xF; code=buf[1]
    mid=(buf[2]<<8)|buf[3]; pos=4
    tok=b""
    if tkl:
        tok=buf[pos:pos+tkl]; pos+=tkl
    payload=b""
    while pos<len(buf):
        if buf[pos]==0xFF:
            payload=buf[pos+1:]; break
        # skip options (no need to parse for demo)
        b=buf[pos]; pos+=1
        d=(b>>4)&0xF; l=b&0xF
        if d==13: pos+=1
        elif d==14: pos+=2
        if l==13: pos+=1
        elif l==14: pos+=2
        # skip value
        # (we omit bounds checks for brevity; server is trusted)
    return dict(ver=ver, type=typ, code=code, mid=mid, token=tok, payload=payload)

def interactive(host, port):
    s=socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    print("Cliente CoAP listo.\nComandos:\n  GET /<recurso>\n  POST /<recurso> {json}\n  PUT /<recurso> {json}\n  DELETE /<recurso>\n  non (toggle CON/NON)\n  q (salir)")
    use_non=False
    while True:
        try:
            line=input("> ").strip()
        except EOFError:
            break
        if not line: continue
        if line.lower()=="q": break
        if line.lower()=="non":
            use_non=not use_non
            print("Modo:", "NON" if use_non else "CON"); continue
        parts=line.split(maxsplit=2)
        if len(parts)<2: print("Formato: <METHOD> /path [json]"); continue
        method, path = parts[0].upper(), parts[1]
        body=b""
        if len(parts)==3:
            try:
                body=json.dumps(json.loads(parts[2])).encode()
            except Exception:
                print("JSON inválido"); continue
        if   method=="GET":    code=GET
        elif method=="POST":   code=POST
        elif method=="PUT":    code=PUT
        elif method=="DELETE": code=DELETE
        else: print("Método no soportado"); continue
        mid=random.randint(0,0xFFFF)
        msg, tok = build_msg(TYPE_NON if use_non else TYPE_CON, code, mid, path, body)
        s.sendto(msg,(host,port))
        s.settimeout(2.0)
        try:
            buf,_=s.recvfrom(2048)
        except socket.timeout:
            print("Timeout"); continue
        rep=parse_reply(buf)
        if not rep: print("Respuesta inválida"); continue
        print(f"type={rep['type']} code=0x{rep['code']:02X} mid=0x{rep['mid']:04X}")
        pl=rep['payload'].decode(errors='ignore')
        if pl: print("payload:", pl)

if __name__=="__main__":
    ap=argparse.ArgumentParser()
    ap.add_argument("host"); ap.add_argument("port", type=int)
    a=ap.parse_args()
    interactive(a.host, a.port)
