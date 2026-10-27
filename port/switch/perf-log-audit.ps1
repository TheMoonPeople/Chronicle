# Offline only: reads saved reports, estimates cumulative reported-tick coverage and identifies worst windows.
param([string]$AuditLogPath = '', [int]$AuditWindowCount = 5)

function Get-PerfWindowAudit([string[]]$ReportLines, [int]$WindowCount = 5) {
    $reports = [Collections.Generic.List[object]]::new()
    [long]$reportedTicks = 0
    foreach ($line in $ReportLines) {
        $row = $line -replace '^.*\] perf ', 'perf '
        if ($row -notmatch '^perf (?<seconds>[\d.]+)s .*ticks (?<ticks>\d+) late (?<late>\d+) interval (?<interval>[\d.]+)/(?<intervalMax>[\d.]+) work (?<work>[\d.]+)/(?<workMax>[\d.]+).*gpu canon (?<gpu>[\d.]+)/(?<gpuMax>[\d.]+)') {
            continue
        }
        $duration = [double]$Matches.seconds
        $count = [long]$Matches.ticks
        $reports.Add([pscustomobject][ordered]@{
            report_index_one_based = $reports.Count + 1
            reported_perf_tick_start_estimate = $reportedTicks
            reported_perf_tick_end_exclusive_estimate = $reportedTicks + $count
            seconds = $duration
            ticks = $count
            late_ticks = [int]$Matches.late
            ticks_per_second = if ($duration -gt 0) { [Math]::Round($count / $duration, 6) } else { 0.0 }
            interval_ms_mean = [double]$Matches.interval
            interval_ms_maximum = [double]$Matches.intervalMax
            work_ms_mean = [double]$Matches.work
            work_ms_maximum = [double]$Matches.workMax
            canonical_gpu_ms_mean = [double]$Matches.gpu
            canonical_gpu_ms_maximum_sample = [double]$Matches.gpuMax
            raw = $row
        })
        $reportedTicks += $count
    }
    $selected = @($reports | Where-Object { $_.seconds -ge 1.0 } | Select-Object -Last $WindowCount)
    [pscustomobject][ordered]@{
        coverage_note = 'Last complete time windows; logical coverage varies with cadence. Bounds are zero-based cumulative ticks printed in parsed perf reports, not asserted game/input ticks. Unreported final partial ticks are absent; printed durations are rounded.'
        gpu_query_lag_note = 'GPU timings come from asynchronously completed queries and can lag CPU-reported ticks; these bounds do not identify exact GPU logical-frame coverage.'
        reported_ticks_total = $reportedTicks
        reports_seen = $reports.Count
        selected_full_windows = $selected.Count
        selected = $selected
        worst_canonical_gpu_mean_window = $selected | Sort-Object canonical_gpu_ms_mean -Descending | Select-Object -First 1
        worst_canonical_gpu_sample_window = $selected | Sort-Object canonical_gpu_ms_maximum_sample -Descending | Select-Object -First 1
        lowest_cadence_window = $selected | Sort-Object ticks_per_second | Select-Object -First 1
        most_late_ticks_window = $selected | Sort-Object late_ticks -Descending | Select-Object -First 1
        all_report_tick_counts = @($reports | Select-Object report_index_one_based, seconds, ticks,
            reported_perf_tick_start_estimate, reported_perf_tick_end_exclusive_estimate)
    }
}

if ($AuditLogPath) {
    Get-PerfWindowAudit -ReportLines @(Get-Content -LiteralPath $AuditLogPath) -WindowCount $AuditWindowCount |
        ConvertTo-Json -Depth 8
}
