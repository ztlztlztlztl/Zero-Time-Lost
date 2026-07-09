import urllib.request
import urllib.parse
import argparse
import sys
import json

# 创建参数解析器
parser = argparse.ArgumentParser(description="这是一个用于获取每日一言的脚本")
parser.add_argument("--tag", required=False, type=str, help="每日一言的标签")

args = parser.parse_args()

base_url = "https://uapis.cn/api/v1/saying/random?category=%E8%AF%97%E8%AF%8D&category=%E6%96%87%E5%AD%A6&category=%E5%BD%B1%E8%A7%86"

if args.tag or args.tag == "无":
    safe_tag = urllib.parse.quote(args.tag)
    url = f"{base_url}&tag={safe_tag}"
else:
    url = base_url

try:
    with urllib.request.urlopen(url) as response:
        res_str = response.read().decode('utf-8')
        json_data = json.loads(res_str)
        print(json_data["content"], flush=True)

except Exception as e:
    print("愿你千帆阅尽，归来仍是少年", flush=True)