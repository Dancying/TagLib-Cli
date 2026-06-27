#!/usr/bin/env python3
import json
import os

INPUT_JSON = "results.json"
OUTPUT_HTML = "report.html"

HTML_TEMPLATE = """<!DOCTYPE html>
<html lang="zh-CN">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Taglib-CLI 自动化测试报告</title>
    <style>
        :root {
            --primary-color: #4f46e5;
            --primary-hover: #4338ca;
            --success-color: #10b981;
            --fail-color: #ef4444;
            --bg-color: #f8fafc;
            --card-bg: #ffffff;
            --table-hdr-bg: #f1f5f9;
            --text-main: #1e293b;
            --text-muted: #64748b;
            --border-color: #cbd5e1;
            --terminal-bg: #0f172a;
            --terminal-text: #e2e8f0;
            --row-hover: #f1f5f9;
            --cmd-preview-bg: #f8fafc;
            --cmd-preview-text: #475569;
        }
        * { box-sizing: border-box; margin: 0; padding: 0; }
        body { font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, "Helvetica Neue", Arial, sans-serif; background-color: var(--bg-color); color: var(--text-main); padding: 40px 8%; line-height: 1.5; font-size: 17px; }
        .container { width: 100%; margin: 0 auto; }
        header { margin-bottom: 32px; }
        header h1 { font-size: 34px; color: #0f172a; font-weight: 700; margin-bottom: 10px; }
        header p { color: var(--text-muted); font-size: 17px; }
        
        .summary-grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(220px, 1fr)); gap: 24px; margin-bottom: 32px; }
        .summary-card { background: var(--card-bg); border: 1px solid var(--border-color); padding: 24px; border-radius: 12px; box-shadow: 0 1px 3px rgba(0,0,0,0.05); }
        .summary-card .title { font-size: 17px; color: var(--text-muted); font-weight: 500; margin-bottom: 8px; }
        .summary-card .value { font-size: 38px; font-weight: 700; color: #0f172a; }
        .summary-card.pass .value { color: var(--success-color); }
        .summary-card.fail .value { color: var(--fail-color); }
        
        .filter-bar { margin-bottom: 24px; display: flex; gap: 14px; }
        .btn { padding: 10px 22px; border-radius: 6px; font-size: 17px; font-weight: 500; cursor: pointer; border: 1px solid var(--border-color); background: var(--card-bg); color: var(--text-main); transition: all 0.2s; }
        .btn:hover { background: #f1f5f9; }
        .btn.active { background: var(--primary-color); color: #fff; border-color: var(--primary-color); }
        
        .table-container { background: var(--card-bg); border: 1px solid var(--border-color); border-radius: 12px; overflow: hidden; box-shadow: 0 4px 6px -1px rgba(0,0,0,0.05); }
        table { width: 100%; border-collapse: collapse; text-align: left; font-size: 17px; table-layout: fixed; }
        th { background: var(--table-hdr-bg); padding: 14px 12px; font-weight: 600; color: #334155; border-bottom: 1px solid var(--border-color); font-size: 17px; }
        td { padding: 12px 12px; border-bottom: 1px solid var(--border-color); vertical-align: middle; font-size: 17px; color: var(--text-main); word-break: break-all; white-space: normal; }
        
        /* 列宽精细设置与目标文件列自适应完整显示防止折行 */
        .col-status { width: 90px; text-align: center; }
        .col-file { width: 180px; white-space: nowrap; }
        .col-op { width: 95px; }
        .col-field { width: 150px; }
        .col-content { width: 25%; }
        .col-cmd { width: auto; }
        .col-action { width: 75px; text-align: center; }

        th.col-status, td.col-status { padding-left: 16px; }
        th.col-action, td.col-action { padding-right: 16px; }

        .cell-wrapper { width: 100%; display: block; line-height: 1.4; padding: 2px 0; -webkit-user-select: text; user-select: text; }

        .col-content .cell-wrapper { white-space: nowrap; overflow-x: auto; text-overflow: clip; scrollbar-width: none; -ms-overflow-style: none; }
        .cmd-code-block { background-color: var(--cmd-preview-bg); color: var(--cmd-preview-text); padding: 6px 12px; border-radius: 6px; font-family: ui-monospace, SFMono-Regular, Menlo, Monaco, Consolas, monospace; font-size: 15px; display: inline-block; width: 100%; box-sizing: border-box; border: 1px solid var(--border-color); white-space: nowrap; overflow-x: auto; word-break: keep-all; scrollbar-width: none; -ms-overflow-style: none; }
        .col-cmd .cell-wrapper { overflow-x: auto; scrollbar-width: none; -ms-overflow-style: none; }
        .col-content .cell-wrapper::-webkit-scrollbar, .col-cmd .cell-wrapper::-webkit-scrollbar, .cmd-code-block::-webkit-scrollbar { display: none !important; width: 0 !important; height: 0 !important; }

        .toggle-btn { background: #f1f5f9; border: 1px solid var(--border-color); color: var(--text-main); font-size: 13px; font-weight: 600; padding: 5px 10px; border-radius: 4px; cursor: pointer; transition: all 0.2s; display: inline-flex; align-items: center; justify-content: center; width: 48px; }
        .toggle-btn:hover { background: var(--primary-color); border-color: var(--primary-color); color: #fff; }
        .toggle-btn.open { background: var(--fail-color); border-color: var(--fail-color); color: #fff; }
        
        tr.detail-row { background: #f8fafc; display: none; }
        tr.detail-row td { padding: 0; border-bottom: 1px solid var(--border-color); white-space: normal; overflow: visible; }
        tr.main-row { transition: background 0.2s; }
        tr.main-row:hover { background: var(--row-hover); }
        
        .badge { display: inline-flex; align-items: center; padding: 4px 10px; border-radius: 4px; font-size: 14px; font-weight: 600; }
        .badge.pass { background-color: #dcfce7; color: #166534; border: 1px solid #bbf7d0; }
        .badge.fail { background-color: #fee2e2; color: #991b1b; border: 1px solid #fca5a5; }
        
        .split-detail-wrapper { display: table; width: 100%; table-layout: fixed; background-color: #fdfefe; }
        .detail-left-pane { display: table-cell; width: 42%; vertical-align: top; padding: 26px 24px; border-right: 1px solid var(--border-color); background-color: #ffffff; }
        .detail-right-pane { display: table-cell; width: 58%; vertical-align: top; background-color: #ffffff; padding: 26px 32px; }
        
        .detail-left-pane h4, .detail-right-pane h4 { font-size: 18px; color: #0f172a; margin-bottom: 20px; border-left: 4px solid var(--primary-color); padding-left: 12px; font-weight: 700; }
        
        .terminal-panel-box { margin-bottom: 20px; border: 1px solid var(--border-color); border-radius: 8px; overflow: hidden; background: var(--terminal-bg); box-shadow: 0 4px 6px -1px rgba(0,0,0,0.1); }
        .terminal-panel-box:last-child { margin-bottom: 0; }
        .panel-header { background: #1e293b; padding: 8px 16px; font-size: 14px; font-weight: 600; color: #cbd5e1; display: flex; justify-content: space-between; align-items: center; border-bottom: 1px solid #334155; }
        
        pre { background: var(--terminal-bg); color: var(--terminal-text); padding: 16px; font-family: ui-monospace, SFMono-Regular, Menlo, Monaco, Consolas, monospace; font-size: 15px; overflow-x: auto; white-space: pre-wrap; margin: 0; max-height: 250px; overflow-y: auto; -webkit-user-select: text; user-select: text; }
        pre::-webkit-scrollbar { width: 6px; height: 6px; }
        pre::-webkit-scrollbar-thumb { background: #475569; border-radius: 3px; }
        
        .info-item { font-size: 16px; margin-bottom: 12px; color: var(--text-muted); line-height: 1.6; }
        .info-item strong { color: #1e293b; font-weight: 600; display: inline-block; min-width: 140px; }
        .info-item .mono-text { font-family: ui-monospace, Consolas, monospace; background: #f1f5f9; padding: 3px 8px; border-radius: 4px; font-size: 15px; color: #0f172a; border: 1px solid var(--border-color); word-break: break-all; -webkit-user-select: text; user-select: text; }
    </style>
</head>
<body>
    <div class="container">
        <header>
            <h1>Taglib-CLI 自动化测试报告</h1>
            <p>沙盒工作环境: <span id="sandbox-dir">--</span></p>
        </header>

        <div class="summary-grid">
            <div class="summary-card"><div class="title">总自动化用例数</div><div class="value" id="sum-total">0</div></div>
            <div class="summary-card pass"><div class="title">通过用例</div><div class="value" id="sum-passed">0</div></div>
            <div class="summary-card fail"><div class="title">失败用例</div><div class="value" id="sum-failed">0</div></div>
            <div class="summary-card"><div class="title">综合通过率</div><div class="value" id="sum-rate">0%</div></div>
        </div>

        <div class="filter-bar">
            <button class="btn active" onclick="filterTable('all', this)">全部用例</button>
            <button class="btn" onclick="filterTable('通过', this)">仅显示通过</button>
            <button class="btn" onclick="filterTable('失败', this)">仅显示失败</button>
        </div>

        <div class="table-container">
            <table>
                <thead>
                    <tr>
                        <th class="col-status">状态</th>
                        <th class="col-file">目标文件</th>
                        <th class="col-op">操作类型</th>
                        <th class="col-field">操作字段</th>
                        <th class="col-content">测试内容</th>
                        <th class="col-cmd">执行命令</th>
                        <th class="col-action">详细</th>
                    </tr>
                </thead>
                <tbody id="test-tbody">
                </tbody>
            </table>
        </div>
    </div>

    <script>
        const reportData = $REPORT_JSON_DATA$;

        function initReport() {
            document.getElementById('sandbox-dir').innerText = reportData.summary.sandbox_dir;
            document.getElementById('sum-total').innerText = reportData.summary.total;
            document.getElementById('sum-passed').innerText = reportData.summary.passed;
            document.getElementById('sum-failed').innerText = reportData.summary.failed;
            document.getElementById('sum-rate').innerText = reportData.summary.pass_rate;

            const tbody = document.getElementById('test-tbody');
            reportData.details.forEach((item, index) => {
                const mainRow = document.createElement('tr');
                mainRow.className = "main-row row-" + item.状态;

                const statusBadge = item.状态 === '通过' ? '<span class="badge pass">通过</span>' : '<span class="badge fail">失败</span>';
                
                mainRow.innerHTML = '<td class="col-status">' + statusBadge + '</td>' +
                    '<td class="col-file"><span class="cell-wrapper"><strong>' + item.文件名 + '</strong></span></td>' +
                    '<td class="col-op"><span class="cell-wrapper">' + item.操作 + '</span></td>' +
                    '<td class="col-field"><span class="cell-wrapper"><code>' + item.字段 + '</code></span></td>' +
                    '<td class="col-content"><span class="cell-wrapper">' + escapeHtml(item.content || item.内容) + '</span></td>' +
                    '<td class="col-cmd"><span class="cell-wrapper"><span class="cmd-code-block">' + escapeHtml(item.命令) + '</span></span></td>' +
                    '<td class="col-action"><button class="toggle-btn" id="btn-' + index + '" onclick="toggleDetail(' + index + ')">展开</button></td>';

                const detailRow = document.createElement('tr');
                detailRow.className = 'detail-row';
                detailRow.id = "detail-" + index;

                const d = item.details || item.详细;
                const md5Badge = d["md5 校验状态"] === '未改变' ? '<span class="badge pass">未改变 (流安全)</span>' : '<span class="badge fail">已改变 (⚠️核心损坏)</span>';
                
                let imgMd5Section = '';
                if (item.操作 === '导出' || (d["导入前图片 md5"] && d["导入前图片 md5"] !== 'N/A')) {
                    const imgMatchBadge = d["导入前图片 md5"] === d["导出后图片 md5"] ? '<span class="badge pass">一致 (OK)</span>' : '<span class="badge fail">不一致 (⚠️图片损坏)</span>';
                    imgMd5Section = '<div class="info-item"><strong>导入前图片 MD5:</strong><span class="mono-text">' + d["导入前图片 md5"] + '</span></div>' +
                                    '<div class="info-item"><strong>导出后图片 MD5:</strong><span class="mono-text">' + d["导出后图片 md5"] + '</span></div>' +
                                    '<div class="info-item"><strong>图片哈希一致性:</strong>' + imgMatchBadge + '</div>';
                }

                const streams = d["taglib-cli 在终端执行的输入输出内容"] || {};
                const mainOp = streams["主要操作结果"] || {};
                const beforeRead = streams["修改前读取结果 [-r " + item.字段 + "]"] || {};
                const afterRead = streams["修改后读取验证结果 [-r " + item.字段 + "]"] || {};

                detailRow.innerHTML = '<td colspan="7">' +
                    '<div class="split-detail-wrapper">' +
                        '<div class="detail-left-pane">' +
                            '<h4>基础状态与完整性看板</h4>' +
                            '<div class="info-item"><strong>用例别名:</strong> ' + d.用例名称 + '</div>' +
                            '<div class="info-item"><strong>原始音频源名称:</strong> ' + d.音频原始文件名 + '</div>' +
                            '<div class="info-item"><strong>工作沙盒路径:</strong> <span class="mono-text" style="font-size:13px; color:#475569;">' + d.音频原始绝对路径 + '</span></div>' +
                            '<div class="info-item"><strong>操作前 audio md5:</strong> <span class="mono-text">' + d["操作前 audio md5"] + '</span></div>' +
                            '<div class="info-item"><strong>操作后 audio md5:</strong> <span class="mono-text">' + d["操作后 audio md5"] + '</span></div>' +
                            '<div class="info-item"><strong>音频比特流状态:</strong> ' + md5Badge + '</div>' +
                            imgMd5Section + 
                        '</div>' +
                        
                        '<div class="detail-right-pane">' +
                            '<h4>控制流终端交互全记录 (taglib-cli)</h4>' +
                            
                            '<div class="terminal-panel-box">' +
                                '<div class="panel-header"><span>① 修改前事前数据拉取 [-r ' + item.字段 + ']</span><small>返回码: ' + (beforeRead.返回码 !== null && beforeRead.返回码 !== undefined ? beforeRead.返回码 : '--') + ' | 耗时: ' + (beforeRead.耗时_ms || 0) + 'ms</small></div>' +
                                '<pre>' + (escapeHtml(beforeRead.标准输出 || beforeRead.标准错误) || '[前置节点未执行/无输出]') + '</pre>' +
                            '</div>' +
                            
                            '<div class="terminal-panel-box">' +
                                '<div class="panel-header"><span>② 核心原子事务操作</span><small>返回码: ' + (mainOp.返回码 !== null && mainOp.返回码 !== undefined ? mainOp.返回码 : '--') + ' | 耗时: ' + (mainOp.耗时_ms || 0) + 'ms</small></div>' +
                                '<pre>' + (escapeHtml(mainOp.标准输出 || mainOp.标准错误) || '[完成 - 无显式流输出]') + '</pre>' +
                            '</div>' +
                            
                            '<div class="terminal-panel-box">' +
                                '<div class="panel-header"><span>③ 修改后事后一致性断言 [-r ' + item.字段 + ']</span><small>返回码: ' + (afterRead.返回码 !== null && afterRead.返回码 !== undefined ? afterRead.返回码 : '--') + ' | 耗时: ' + (afterRead.耗时_ms || 0) + 'ms</small></div>' +
                                '<pre>' + (escapeHtml(afterRead.标准输出 || afterRead.标准错误) || '[后置节点未执行/无输出]') + '</pre>' +
                            '</div>' +
                        '</div>' +
                    '</div>' +
                '</td>';

                tbody.appendChild(mainRow);
                tbody.appendChild(detailRow);
            });
        }

        function toggleDetail(index) {
            const target = document.getElementById("detail-" + index);
            const btn = document.getElementById("btn-" + index);
            if (target.style.display === 'table-row') {
                target.style.display = 'none';
                btn.innerText = '展开';
                btn.classList.remove('open');
            } else {
                target.style.display = 'table-row';
                btn.innerText = '收起';
                btn.classList.add('open');
            }
        }

        function filterTable(status, btn) {
            document.querySelectorAll('.filter-bar .btn').forEach(b => b.classList.remove('active'));
            btn.classList.add('active');

            const mainRows = document.querySelectorAll('.main-row');
            const detailRows = document.querySelectorAll('.detail-row');
            
            detailRows.forEach(r => r.style.display = 'none');
            document.querySelectorAll('.toggle-btn').forEach(b => {
                b.innerText = '展开';
                b.classList.remove('open');
            });

            mainRows.forEach(row => {
                if (status === 'all' || row.classList.contains("row-" + status)) {
                    row.style.display = 'table-row';
                } else {
                    row.style.display = 'none';
                }
            });
        }

        function escapeHtml(str) {
            if (!str) return '';
            return str.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;').replace(/\"/g, '&quot;').replace(/'/g, '&#039;');
        }

        window.onload = initReport;
    </script>
</body>
</html>
"""

def main():
    if not os.path.exists(INPUT_JSON):
        print(f"❌ 错误: 未找到测试结果文件 '{INPUT_JSON}'，请先运行测试脚本。")
        return

    with open(INPUT_JSON, "r", encoding="utf-8") as f:
        try:
            data = json.load(f)
        except Exception as e:
            print(f"❌ 错误: 解析 JSON 文件失败: {e}")
            return

    json_string = json.dumps(data, ensure_ascii=False)
    rendered_html = HTML_TEMPLATE.replace("$REPORT_JSON_DATA$", json_string)

    with open(OUTPUT_HTML, "w", encoding="utf-8") as f:
        f.write(rendered_html)

    print(f"▶ 自动化可视化报告生成成功: {os.path.abspath(OUTPUT_HTML)}")

if __name__ == "__main__":
    main()
