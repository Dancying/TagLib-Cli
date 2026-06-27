#!/usr/bin/env python3
import glob
import hashlib
import json
import os
import random
import shutil
import subprocess
import time

# ==========================================
# 全局常量配置
# ==========================================
CLI_BIN = "/app/bin/taglib-cli"
OUTPUT_JSON = "results.json"

SUPPORTED_EXTS = [
    "flac", "ogg", "opus", "spx", "oga", "mp3", "mp4", 
    "m4a", "mka", "webm", "wav", "aif", "aiff"
]
SUPPORTED_IMG_EXTS = ["jpg", "jpeg", "png"]

BASE_TEXT_FIELDS = ["Title", "Artist", "Album", "Album_Artist", "Composer", "Lyricist", "Description", "Genre"]
EXTENDED_FIELDS = ["Compilation", "Lyrics", "Comment"]
IMAGE_TYPES = ["Front_Cover", "Back_Cover", "File_Icon", "During_Performance"]

TEST_DATASETS = [
    {"type": "纯数字", "value": "123456789012345678901234567890123456789012345678901234567890"},
    {"type": "纯字母", "value": "AabcdEFGhigAabcdEFGhigAabcdEFGhigAabcdEFGhigAabcdEFGhigAabcd"},
    {"type": "纯中文", "value": "测试音乐标签测试音乐标签测试音乐标签测试音乐标签测试音乐标签测试音乐标签"},
    {"type": "纯符号", "value": "!@#$%^&*()_+=-[]{}|;':\",./<>?!@#$%^&*()_+=-[]{}|;':\",./<>?"},
    {"type": "混合数据", "value": "音乐123_abc_!@#音乐123_abc_!@#音乐123_abcb_!@#音乐123_abc_!@#"}
]

MULTILINE_DATASETS = [
    {
        "type": "多行真实歌词", 
        "value": "[00:01.00] 这是一个多行歌词测试\n[00:04.50] 模拟真实的歌词流 data\n[00:08.20] 每一行都带有时间戳"
    },
    {
        "type": "多行真实评论", 
        "value": "精选评论：\n1. taglib-cli 表现稳定。\n2. MD5 完整性校验通过。"
    }
]

STANDARD_DATES = {
    "flac": "2024-01-20", 
    "mp4": "2024", "m4a": "2024", "mka": "2024", "webm": "2024",
    "ogg": "2024-01", "opus": "2024-01", "spx": "2024-01", "oga": "2024-01",
    "mp3": "2024-01", "wav": "2024-01", "aif": "2024-01", "aiff": "2024-01"
}


# ==========================================
# 工具辅助函数
# ==========================================
def calculate_file_md5(file_path):
    """计算指定物理文件的真实 MD5 摘要值"""
    if not os.path.exists(file_path):
        return "FILE_NOT_FOUND"
    hasher = hashlib.md5()
    try:
        with open(file_path, "rb") as f:
            for chunk in iter(lambda: f.read(65536), b""):
                hasher.update(chunk)
        return hasher.hexdigest()
    except Exception:
        return "MD5_ERROR"


def get_audio_md5(file_path):
    """调用 CLI 工具获取音频数据流核心部分的 Audio_MD5 签名值"""
    res = run_cli_cmd(["-r", "Audio_MD5", file_path])
    return res["stdout"] if res["returncode"] == 0 else "UNKNOWN_MD5"


def run_cli_cmd(args):
    """底层统一调用 taglib-cli 命令行交互封装"""
    cmd = [CLI_BIN] + args
    start_time = time.time()
    try:
        res = subprocess.run(cmd, capture_output=True, text=True, timeout=5)
        duration = round((time.time() - start_time) * 1000, 2)
        return {
            "returncode": res.returncode,
            "stdout": res.stdout.strip(),
            "stderr": res.stderr.strip(),
            "duration_ms": duration,
        }
    except Exception as e:
        return {"returncode": -1, "stdout": "", "stderr": str(e), "duration_ms": 0.0}


def capture_verify_output(field, target_file):
    """执行 -r 参数读取操作，并捕获标准的终端交互日志"""
    v_res = run_cli_cmd(["-r", field, target_file])
    return {
        "返回码": v_res["returncode"],
        "标准输出": v_res["stdout"],
        "标准错误": v_res["stderr"],
        "耗时_ms": v_res["duration_ms"]
    }


# ==========================================
# 沙盒测试环境初始化
# ==========================================
def init_sandbox(src_dir):
    script_dir = os.path.dirname(os.path.abspath(__file__))
    temp_dir = os.path.join(script_dir, f"temp_{time.strftime('%Y%m%d%H%M%S')}")
    os.makedirs(temp_dir, exist_ok=True)

    copied_audio, copied_images = {}, []

    audio_counter = 1
    for ext in SUPPORTED_EXTS:
        files = glob.glob(os.path.join(src_dir, f"*.{ext}")) + glob.glob(os.path.join(src_dir, f"*.{ext.upper()}"))
        for f in files:
            new_name = f"AUDIO_{audio_counter:03d}.{ext.lower()}"
            dest_path = os.path.join(temp_dir, new_name)
            shutil.copy2(f, dest_path)
            copied_audio.setdefault(ext.lower(), []).append({"original": f, "current": dest_path})
            audio_counter += 1

    img_counter = 1
    for ext in SUPPORTED_IMG_EXTS:
        files = glob.glob(os.path.join(src_dir, f"*.{ext}")) + glob.glob(os.path.join(src_dir, f"*.{ext.upper()}"))
        for f in files:
            new_name = f"IMAGE_{img_counter:03d}.{ext.lower()}"
            dest_path = os.path.join(temp_dir, new_name)
            shutil.copy2(f, dest_path)
            copied_images.append(dest_path)
            img_counter += 1

    return temp_dir, copied_audio, copied_images


# ==========================================
# 测试用例核心执行引擎
# ==========================================
def execute_test_case(case, progress_str):
    """核心用例调度与全景状态断言引擎"""
    file_info = case["file_info"]
    target_file = file_info["current"]
    field = case["field"]
    
    # 1. 显式记录操作前的原始音频流 Audio_MD5
    initial_audio_md5 = get_audio_md5(target_file)

    # 2. 执行核心修改操作前的【事前数据拉取】
    before_terminal_output = capture_verify_output(field, target_file)

    # 3. 如果是用例要求前置填充，先执行填充动作
    if case.get("setup_args"):
        for fill_arg in case["setup_args"]:
            run_cli_cmd(fill_arg + [target_file])

    # 4. 组装并运行主要操作命令
    full_args = case["action_args"] + [target_file]
    action_command_str = f"{CLI_BIN} " + " ".join(full_args)
    
    print(f"{progress_str} {action_command_str}")
    action_res = run_cli_cmd(full_args)
    
    status = "通过"
    reasons = []

    if action_res["returncode"] != 0:
        status = "失败"
        reasons.append(f"Command exited with non-zero code {action_res['returncode']}")

    # 5. 执行核心修改操作后的【事后数据拉取验证】
    after_terminal_output = capture_verify_output(field, target_file)
    v_stdout = after_terminal_output["标准输出"]

    # 6. 用例逻辑检验断言（仅针对有强断言要求的文本或删除类型的操作）
    if case.get("require_strict_verify", True) and status == "通过":
        if case["action_type"] == "删除" and v_stdout:
            status = "失败"
            reasons.append(f"Tag should be deleted, but read text: '{v_stdout}'")
        elif case["action_type"] == "写入" and case.get("expected_verify_val") is not None:
            if v_stdout != case["expected_verify_val"]:
                status = "失败"
                reasons.append(f"Verification mismatch. Expected: '{case['expected_verify_val']}', Got: '{v_stdout}'")

    # 7. 处理图片级 MD5 的多维展示与断言
    src_image_md5 = "N/A"
    dst_image_md5 = "N/A"
    
    if field in IMAGE_TYPES and case.get("src_image_path"):
        src_image_md5 = calculate_file_md5(case["src_image_path"])

    if case["action_type"] == "导出":
        dst_image_md5 = calculate_file_md5(case["content"])
        if status == "通过" and src_image_md5 != dst_image_md5:
            status = "失败"
            reasons.append(f"Exported image MD5 mismatch! Src: {src_image_md5} | Dst: {dst_image_md5}")

    # 8. 显式记录操作后的音频比特流 Audio_MD5 完整性校验
    post_audio_md5 = get_audio_md5(target_file)
    md5_status = "未改变" if initial_audio_md5 == post_audio_md5 else "已改变"
    if md5_status == "已改变":
        status = "失败"
        reasons.append(f"Audio_MD5 changed from {initial_audio_md5} to {post_audio_md5}")

    final_status = status

    return {
        "状态": final_status,
        "文件名": os.path.basename(target_file),
        "操作": case["action_type"],
        "字段": field,
        "内容": case["content"],
        "命令": action_command_str,
        "详细": {
            "用例名称": case["name"],
            "音频原始文件名": os.path.basename(file_info["original"]),
            "音频原始绝对路径": file_info["original"],
            "操作前 audio md5": initial_audio_md5,
            "操作后 audio md5": post_audio_md5,
            "md5 校验状态": md5_status,
            "用例状态": final_status,
            "导入前图片 md5": src_image_md5,
            "导出后图片 md5": dst_image_md5,
            "taglib-cli 在终端执行的输入输出内容": {
                "主要操作结果": {
                    "返回码": action_res["returncode"],
                    "标准输出": action_res["stdout"],
                    "标准错误": action_res["stderr"],
                    "耗时_ms": action_res["duration_ms"]
                },
                f"修改前读取结果 [-r {field}]": before_terminal_output,
                f"修改后读取验证结果 [-r {field}]": after_terminal_output
            }
        }
    }


# ==========================================
# 测试队列构建器 (用例集装配)
# ==========================================
def build_base_fields_queue(audio_pool, temp_dir, sample_img):
    queue = []
    all_text_fields = BASE_TEXT_FIELDS + EXTENDED_FIELDS

    for fmt, files in audio_pool.items():
        # 遍历该音频格式下的每一个文件，确保所有文件都被充分测试
        for file_info in files:
            
            # ---------------- 文本及拓展属性标签写入测试 ----------------
            for field in all_text_fields:
                if field == "Compilation":
                    if fmt in ["mp4", "m4a", "mka", "webm", "spx", "oga"]:
                        continue
                    for val in ["1", "0"]:
                        queue.append(create_text_case(fmt, file_info, field, val, f"Compilation_{val}"))
                    continue

                target_datasets = MULTILINE_DATASETS if field in ["Lyrics", "Comment"] else TEST_DATASETS

                for data in target_datasets:
                    queue.append(create_text_case(fmt, file_info, field, data["value"], f"Str_{data['type']}"))
                    
                    # 为了防止多线程或并发写入时同名文件冲突，在文件名中引入当前音频文件的 base 名字
                    audio_base_name = os.path.splitext(os.path.basename(file_info["current"]))[0]
                    txt_name = f"file_{audio_base_name}_{field}_{data['type']}.txt"
                    txt_path = os.path.join(temp_dir, txt_name)
                    with open(txt_path, "w", encoding="utf-8") as tf:
                        tf.write(data["value"])
                    
                    rel_path = f"./{os.path.relpath(txt_path, os.getcwd())}"
                    queue.append({
                        "name": f"{fmt.upper()}_{audio_base_name}_Text_Write_File_Relative_{field}_{data['type']}",
                        "file_info": file_info, "action_type": "写入", "field": field, "content": rel_path,
                        "action_args": ["-w", f"{field}={rel_path}"], "require_strict_verify": True, "expected_verify_val": data["value"]
                    })

                    abs_path = os.path.abspath(txt_path)
                    queue.append({
                        "name": f"{fmt.upper()}_{audio_base_name}_Text_Write_File_Absolute_{field}_{data['type']}",
                        "file_info": file_info, "action_type": "写入", "field": field, "content": abs_path,
                        "action_args": ["-w", f"{field}={abs_path}"], "require_strict_verify": True, "expected_verify_val": data["value"]
                    })

            # ---------------- 专属数字和特定日期边界写入测试 ----------------
            special_numerics = [("Track_Number", "5"), ("Track_Number", "5/12"), ("Disc_Number", "1"), ("Disc_Number", "1/2")]
            for fld, val in special_numerics:
                queue.append({
                    "name": f"{fmt.upper()}_{os.path.splitext(os.path.basename(file_info['current']))[0]}_Special_Numeric_{fld}_{val.replace('/', '_')}",
                    "file_info": file_info, "action_type": "写入", "field": fld, "content": val,
                    "action_args": ["-w", f"{fld}={val}"], "require_strict_verify": True, "expected_verify_val": val
                })

            valid_date = STANDARD_DATES.get(fmt, "2024-01-20")
            queue.append({
                "name": f"{fmt.upper()}_{os.path.splitext(os.path.basename(file_info['current']))[0]}_Date_Standard_Valid",
                "file_info": file_info, "action_type": "写入", "field": "Date", "content": valid_date,
                "action_args": ["-w", f"Date={valid_date}"], "require_strict_verify": True, "expected_verify_val": valid_date
            })
            queue.append({
                "name": f"{fmt.upper()}_{os.path.splitext(os.path.basename(file_info['current']))[0]}_Date_NonStandard_String",
                "file_info": file_info, "action_type": "写入", "field": "Date", "content": "2024年1月_很久很久以前",
                "action_args": ["-w", f"Date=2024年1月_很久很久以前"], "require_strict_verify": True, "expected_verify_val": "2024年1月_很久很久以前"
            })

            # ---------------- 多类型图像标签高级功能测试 ----------------
            for img_type in IMAGE_TYPES:
                audio_base_name = os.path.splitext(os.path.basename(file_info["current"]))[0]
                extracted_path = os.path.join(temp_dir, f"extracted_{audio_base_name}_{img_type}.jpg")
                
                queue.append({
                    "name": f"{fmt.upper()}_{audio_base_name}_Image_Inject_{img_type}",
                    "file_info": file_info, "action_type": "写入", "field": img_type, "content": sample_img,
                    "action_args": ["-i", f"{img_type}={sample_img}"], "require_strict_verify": False, "src_image_path": sample_img
                })
                queue.append({
                    "name": f"{fmt.upper()}_{audio_base_name}_Image_Extract_{img_type}",
                    "file_info": file_info, "action_type": "导出", "field": img_type, "content": extracted_path,
                    "action_args": ["-e", f"{img_type}={extracted_path}"], "require_strict_verify": False, "src_image_path": sample_img
                })
                queue.append({
                    "name": f"{fmt.upper()}_{audio_base_name}_Image_Write_{img_type}",
                    "file_info": file_info, "action_type": "写入", "field": img_type, "content": sample_img,
                    "action_args": ["-w", f"{img_type}={sample_img}"], "require_strict_verify": False, "src_image_path": sample_img
                })

    return queue


def build_batch_write_scenarios(audio_pool, sample_img):
    """构建复杂的后置组合场景写入测试队列，包含全文本、全图片和图文混合的批量覆盖场景"""
    queue = []
    all_text_fields = BASE_TEXT_FIELDS + EXTENDED_FIELDS
    
    for fmt, files in audio_pool.items():
        # 获取该格式下的第一个文件进行组合覆盖测试
        file_info = files[0]
        audio_base_name = os.path.splitext(os.path.basename(file_info["current"]))[0]
        
        # 兼容处理特定格式不支持 Compilation 字段的情况
        fmt_text_fields = [f for f in all_text_fields if not (f == "Compilation" and fmt in ["mp4", "m4a", "mka", "webm", "spx", "oga"])]

        # ---------------- 场景 1：全文本字段批量一次性写入 ----------------
        all_text_args = []
        for f in fmt_text_fields:
            all_text_args.extend(["-w", f"{f}=BulkText_{f}"])
            
        queue.append({
            "name": f"{fmt.upper()}_{audio_base_name}_Write_All_Text_Fields",
            "file_info": file_info, 
            "action_type": "写入", 
            "field": fmt_text_fields[0],  # 锚定首个字段进行框架验证
            "content": f"全量文本字段批量覆盖写入 (共 {len(fmt_text_fields)} 个)",
            "action_args": all_text_args, 
            "require_strict_verify": True, 
            "expected_verify_val": f"BulkText_{fmt_text_fields[0]}"
        })

        # ---------------- 场景 2：全图片类型批量一次性写入 ----------------
        all_img_args = []
        for img in IMAGE_TYPES:
            all_img_args.extend(["-i", f"{img}={sample_img}"])
            
        queue.append({
            "name": f"{fmt.upper()}_{audio_base_name}_Write_All_Image_Fields",
            "file_info": file_info, 
            "action_type": "写入", 
            "field": IMAGE_TYPES[0], 
            "content": f"全量图片类型批量注入 (共 {len(IMAGE_TYPES)} 个)",
            "action_args": all_img_args, 
            "require_strict_verify": False, 
            "src_image_path": sample_img
        })

        # ---------------- 场景 3：文本 + 图片多字段混合批量写入 ----------------
        mix_args = []
        # 随机抽取 3 个文本字段与 2 个图片字段进行混合批量写入
        mix_texts = random.sample(fmt_text_fields, min(3, len(fmt_text_fields)))
        mix_imgs = random.sample(IMAGE_TYPES, min(2, len(IMAGE_TYPES)))
        
        for f in mix_texts:
            mix_args.extend(["-w", f"{f}=MixBulk_{f}"])
        for img in mix_imgs:
            mix_args.extend(["-i", f"{img}={sample_img}"])
            
        queue.append({
            "name": f"{fmt.upper()}_{audio_base_name}_Write_Mixed_Text_And_Image",
            "file_info": file_info, 
            "action_type": "写入", 
            "field": mix_texts[0], 
            "content": f"图文混合批量写入 (文本: {','.join(mix_texts)} | 图片: {','.join(mix_imgs)})",
            "action_args": mix_args, 
            "require_strict_verify": True, 
            "expected_verify_val": f"MixBulk_{mix_texts[0]}",
            "src_image_path": sample_img
        })

    return queue


def build_batch_delete_scenarios(audio_pool, sample_img):
    """构建复杂的后置组合场景删除测试队列，包含随机填充和级联多重场景"""
    queue = []
    all_text_fields = BASE_TEXT_FIELDS + EXTENDED_FIELDS
    
    for fmt, files in audio_pool.items():
        file_info = files[0]
        
        # 兼容处理特定格式不支持 Compilation 字段的情况
        fmt_text_fields = [f for f in all_text_fields if not (f == "Compilation" and fmt in ["mp4", "m4a", "mka", "webm", "spx", "oga"])]

        # 随机抽取多个文本字段同时删除
        text_samples = random.sample(fmt_text_fields, min(4, len(fmt_text_fields)))
        setup_args = [["-w", f"{f}=FillText_{random.randint(10,99)}"] for f in text_samples]
        action_args = []
        for f in text_samples:
            action_args.extend(["-d", f])
            
        queue.append({
            "name": f"{fmt.upper()}_Delete_Multi_Text_Fields",
            "file_info": file_info, "action_type": "删除", "field": text_samples[0], "content": f"同时删除: {','.join(text_samples)}",
            "setup_args": setup_args, "action_args": action_args, "require_strict_verify": True, "expected_verify_val": ""
        })

        # 随机抽取多个图片字段同时删除
        img_samples = random.sample(IMAGE_TYPES, min(2, len(IMAGE_TYPES)))
        setup_args = [["-i", f"{img}= {sample_img}"] for img in img_samples]
        action_args = []
        for img in img_samples:
            action_args.extend(["-d", img])
            
        queue.append({
            "name": f"{fmt.upper()}_Delete_Multi_Image_Fields",
            "file_info": file_info, "action_type": "删除", "field": img_samples[0], "content": f"同时删除: {','.join(img_samples)}",
            "setup_args": setup_args, "action_args": action_args, "require_strict_verify": False, "src_image_path": sample_img
        })

        # 文本+图片字段（随机抽取）混合同时删除
        mix_text = random.choice(fmt_text_fields)
        mix_img = random.choice(IMAGE_TYPES)
        setup_args = [
            ["-w", f"{mix_text}=MixTextValue"],
            ["-i", f"{mix_img}={sample_img}"]
        ]
        action_args = ["-d", mix_text, "-d", mix_img]
        
        queue.append({
            "name": f"{fmt.upper()}_Delete_Mixed_Text_And_Image",
            "file_info": file_info, "action_type": "删除", "field": mix_text, "content": f"混合删除: {mix_text},{mix_img}",
            "setup_args": setup_args, "action_args": action_args, "require_strict_verify": True, "expected_verify_val": ""
        })

        # 删除全部元数据 (将所有支持的字段和图片全部填充后，一次性调用全部 -d)
        all_setup_args = []
        all_action_args = []
        
        for f in fmt_text_fields:
            all_setup_args.append(["-w", f"{f}=ClearAllStub"])
            all_action_args.extend(["-d", f])
        for img in IMAGE_TYPES:
            all_setup_args.append(["-i", f"{img}={sample_img}"])
            all_action_args.extend(["-d", img])
            
        # 补充常规日期和轨道属性清除
        for extra_fld in ["Track_Number", "Disc_Number", "Date"]:
            all_setup_args.append(["-w", f"{extra_fld}=1"])
            all_action_args.extend(["-d", extra_fld])

        queue.append({
            "name": f"{fmt.upper()}_Delete_All_Metadata_Fields",
            "file_info": file_info, "action_type": "删除", "field": "Title", "content": "全量元数据级联擦除",
            "setup_args": all_setup_args, "action_args": all_action_args, "require_strict_verify": True, "expected_verify_val": ""
        })

    return queue


def create_text_case(fmt, file_info, field, value, sub_type):
    return {
        "name": f"{fmt.upper()}_Text_Write_{sub_type}_{field}",
        "file_info": file_info, "action_type": "写入", "field": field, "content": value,
        "action_args": ["-w", f"{field}={value}"], "require_strict_verify": True, "expected_verify_val": value
    }


# ==========================================
# 自动化测试主执行流流程
# ==========================================
def main():
    user_path = input("🎵 请输入音乐文件夹的绝对路径: ").strip()
    if not os.path.isdir(user_path):
        print("❌ 错误路径，程序退出。")
        return

    temp_dir, audio_pool, image_pool = init_sandbox(user_path)
    if not sum(len(v) for v in audio_pool.values()):
        print("⚠ 未在源目录中检索到任何支持的音频测试数据。")
        return

    sample_img = image_pool[0] if image_pool else os.path.join(temp_dir, "mock_cover.jpg")
    if not image_pool:
        with open(sample_img, "wb") as f:
            f.write(b"fake image stream data")

    primary_queue = build_base_fields_queue(audio_pool, temp_dir, sample_img)
    write_queue = build_batch_write_scenarios(audio_pool, sample_img)
    delete_queue = build_batch_delete_scenarios(audio_pool, sample_img)
    
    full_test_queue = primary_queue + write_queue + delete_queue
    total_cases = len(full_test_queue)
    results = []

    for idx, case in enumerate(full_test_queue, 1):
        progress_str = f"[{idx}/{total_cases}]"
        res_data = execute_test_case(case, progress_str)
        results.append(res_data)

    passed = sum(1 for r in results if r["状态"] == "通过")
    report_data = {
        "summary": {
            "total": total_cases,
            "passed": passed,
            "failed": total_cases - passed,
            "pass_rate": f"{round((passed / total_cases) * 100, 2)}%" if total_cases else "0%",
            "sandbox_dir": temp_dir,
        },
        "details": results,
    }

    with open(OUTPUT_JSON, "w", encoding="utf-8") as f:
        json.dump(report_data, f, indent=4, ensure_ascii=False)
        
    print(f"\n✨ 自动化测试矩阵执行完毕。总计: {total_cases} | 通过: {passed} | 报告已写出至: {OUTPUT_JSON}")


if __name__ == "__main__":
    main()
