import imaplib
import argparse
import json

parser = argparse.ArgumentParser(description="这是一个用于检查邮箱信息是否正确的脚本")
parser.add_argument("--address", required=True, type=str, help="邮箱地址")
parser.add_argument("--code", required=True, type=str, help="IMAP授权码")
parser.add_argument("--server", required=True, type=str, help="服务器地址")
parser.add_argument("--port", required=True, type=str, help="端口号")

args = parser.parse_args()

try:
    mail = imaplib.IMAP4_SSL(args.server, int(args.port), timeout=10)
    mail.login(args.address, args.code)
    mail.logout()
    print(json.dumps({"status": "SUCCESS"}), flush=True)
except Exception as e:
    print(json.dumps({"status": "FAILED", "message": str(e)}), flush=True)