import imaplib
import email
from email.header import decode_header
import argparse
import json

parser = argparse.ArgumentParser(description="这是一个用于拉取未读邮件的脚本")
parser.add_argument("--address", required=True, type=str, help="邮箱地址")
parser.add_argument("--code", required=True, type=str, help="IMAP授权码")
parser.add_argument("--server", required=True, type=str, help="服务器地址")
parser.add_argument("--port", required=True, type=str, help="端口号")
parser.add_argument("--count", required=False, type=int, default=1, help="拉取邮件的数量，默认为1")

args = parser.parse_args()



def decode_str(s):
    if not s:
        return "无主题"
    value, charset = decode_header(s)[0]
    if charset:
        value = value.decode(charset, errors='ignore')
    elif isinstance(value, bytes):
        value = value.decode('utf-8', errors='ignore')
    return str(value)

try:
    mail = imaplib.IMAP4_SSL(args.server, int(args.port), timeout=10) # 找到邮箱服务器
    mail.login(args.address, args.code) # 登录邮箱
    status, data = mail.select("INBOX", readonly=False)
    if status == "OK":
        status, mails = mail.search(None, "UNSEEN")
        mail_ids = mails[0].split()

        result = {}
        
        if mail_ids:
            last_mail_id = mail_ids[-1]
            status, msg_data = mail.fetch(last_mail_id, "(RFC822.HEADER)")
            mail.store(last_mail_id, '+FLAGS', '\\Seen')
            
            for response_part in msg_data:
                if isinstance(response_part, tuple):
                    msg = email.message_from_bytes(response_part[1])
                    subject = decode_str(msg.get("Subject"))
                    sender = decode_str(msg.get("From"))
                    date = str(msg.get("Date"))
                    if "<" in sender and ">" in sender:
                        sender = sender.split("<")[0].strip() or sender
                    
                    result = {
                        "id": last_mail_id.decode('utf-8'),
                        "subject": subject,
                        "from": sender,
                        "date": date
                    }
            print(json.dumps({"status": "SUCCESS", "data": result}, ensure_ascii=False), flush=True)
        else:
            print(json.dumps({"status": "NONE"}, ensure_ascii=False), flush=True)

                
            
    mail.logout()
    

except Exception as e:
    print(json.dumps({"status": "FAILED", "message": str(e)}), flush=True)












