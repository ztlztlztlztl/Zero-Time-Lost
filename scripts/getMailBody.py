import imaplib
import email
import json
import sys
import argparse
import sys
import io

sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8')
sys.stderr = io.TextIOWrapper(sys.stderr.buffer, encoding='utf-8')

parser = argparse.ArgumentParser(description="这是一个用于拉取一封邮件正文的脚本")
parser.add_argument("--address", required=True, type=str, help="邮箱地址")
parser.add_argument("--code", required=True, type=str, help="IMAP授权码")
parser.add_argument("--server", required=True, type=str, help="服务器地址")
parser.add_argument("--port", required=True, type=str, help="端口号")
parser.add_argument("--uid", required=True, type=int, help="邮件的 UID")

args = parser.parse_args()

def extract_body(msg):
    body_text = ""
    body_html = ""
    for part in msg.walk():
        if part.is_multipart() or part.get_filename() is not None:
            continue
        content_type = part.get_content_type()
        if content_type in ["text/plain", "text/html"]:
            raw_bytes = part.get_payload(decode=True)
            if raw_bytes is None:
                continue
            charset = part.get_content_charset() or 'utf-8'
            
            try:
                decoded_str = raw_bytes.decode(charset, errors='replace')
                if content_type == "text/plain":
                    body_text += decoded_str
                elif content_type == "text/html":
                    body_html += decoded_str
                    
            except Exception as e:
                continue

    final_body = body_html if body_html else body_text
    return final_body.strip()

try:
    mail = imaplib.IMAP4_SSL(args.server, int(args.port), timeout=10)
    mail.login(args.address, args.code)
    mail.select("INBOX", readonly=True)
    
    status, data = mail.fetch(str(args.uid), "(RFC822)")
    if status == "OK" and data:
        print(1)
        msg = email.message_from_bytes(data[0][1])
        print(2)
        body = extract_body(msg)
        print(json.dumps({
            "status": "success",
            "body": body
        }, ensure_ascii=False))
    else:
        print(json.dumps({
            "status": "error",
            "message": "无法获取邮件内容"
        }, ensure_ascii=False))
    mail.logout()

except Exception as e:
    print(json.dumps({"status": "error", "message": str(e)}, ensure_ascii=False))

