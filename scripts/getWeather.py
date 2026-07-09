import urllib.request
import argparse

# 创建参数解析器
parser = argparse.ArgumentParser(description="这是一个用于同步城市天气配置的脚本")
parser.add_argument("--code", required=True, type=int, help="6位城市编码")
parser.add_argument("--key", required=True, help="高德api key")

args = parser.parse_args()
url = f"https://restapi.amap.com/v3/weather/weatherInfo?city={args.code}&key={args.key}&extensions=base"

with urllib.request.urlopen(url) as response:
    res = response.read().decode('utf-8')
    print(res, flush=True)