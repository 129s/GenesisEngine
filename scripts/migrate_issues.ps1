param(
  [switch]$Apply,
  [string]$Repo
)

$issues = @(
  @{ Title = 'Runtime: 双缓冲 SimulationSnapshot（线程安全）';
     Labels = 'area/runtime,P0,telemetry';
     Body = @"
参考：docs/roadmap/README.md（近期 P0），include/genesis/runtime/Runtime.hpp，src/runtime/Runtime.cpp
目标：在模拟线程与前端之间提供双缓冲快照，定义捕获点与生命周期；扩展 Runtime API 以获取稳定快照。
验收：长时无竞态，CLI/GUI 安全拉取；并发读单测覆盖。
"@ },
  @{ Title = 'Sandbox CLI: 基本控制命令（pause/resume/step/inspect）';
     Labels = 'area/cli,P0,ux';
     Body = @"
参考：src/sandbox/CliApp.cpp, docs/guides/sandbox-cli.md
目标：实现并文档化 pause/resume/step <n>/inspect agent <id>；inspect 输出位置/当前行动/需求。
验收：脚本化断言命令行为，文档含示例输出。
"@ },
  @{ Title = '指南：sandbox_ui / game 运行时接入指引';
     Labels = 'area/docs,P1,guides';
     Body = @"
参考：docs/architecture/README.md（运行时封装）、docs/guides/sandbox-cli.md
目标：撰写运行时接入指南（快照协议、API、示例代码），约定版本冻结点。
验收：新增 docs/guides/runtime-integration.md，含最小可运行示例。
"@ }
)

foreach ($i in $issues) {
  $repoArg = if ($Repo) { "--repo `"$Repo`"" } else { '' }
  $cmd = "gh issue create --title `"$($i.Title)`" --body `"$($i.Body)`" --label `"$($i.Labels)`" $repoArg"
  if ($Apply) {
    Write-Host "Creating: $($i.Title)" -ForegroundColor Cyan
    $null = cmd /c $cmd
  } else {
    Write-Output $cmd
  }
}