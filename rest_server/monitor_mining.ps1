# Start mining
$ErrorActionPreference = "Stop"
$headers = @{ "Content-Type" = "application/json" }
$body = @{
    hash = "073ff90209cde3a9ce4ccd23598fd1b50e6a1fe34f30bd7240587f0bde6f65af"
    addr1 = "00e72f85c49171825a87a84142bdf02022a75479"
    addr2 = "2ecaa57da5fcc9d45bc2d512eeb2b3e98e0393b7"
    value = 3768
    target = "000000ffffff0000000000000000000000000000000000000000000000000000"
    time_limit = 30  # No time limit
} | ConvertTo-Json

function Get-ErrorResponse {
    param (
        [Parameter(Mandatory=$true)]
        [System.Management.Automation.ErrorRecord]$ErrorRecord
    )
    
    try {
        $responseBody = $ErrorRecord.ErrorDetails.Message
        if ($responseBody) {
            $errorObj = $responseBody | ConvertFrom-Json
            return $errorObj.detail
        }
    }
    catch {
        # If we can't parse the error response, return the original error message
    }
    return $ErrorRecord.Exception.Message
}

try {
    Write-Host "Starting mining..."
    try {
        $response = Invoke-RestMethod -Method Post -Uri "http://localhost:8001/mine/start" -Headers $headers -Body $body
    }
    catch {
        $errorMessage = Get-ErrorResponse $_
        throw "Failed to start mining: $errorMessage"
    }

    if (-not $response.session_id) {
        throw "Error: No session ID received from server"
    }
    
    $session_id = $response.session_id
    Write-Host "Mining session started with ID: $session_id"

    # Monitor status until solution is found
    Write-Host "`nMonitoring mining status..."
    while ($true) {
        try {
            $status = Invoke-RestMethod -Method Get -Uri "http://localhost:8001/mine/$session_id/status"
            if (-not $status) {
                Write-Host "`nError: Received empty status response"
                Start-Sleep -Seconds 5
                continue
            }

            Write-Host ("`rHash Rate: {0:N2} MH/s, Total Hashes: {1:N0}, Current Nonce: {2}" -f ($status.hash_rate / 1000000), $status.total_hashes, $status.current_nonce) -NoNewline
            
            if ($status.solution_found) {
                Write-Host "`n`nSolution found!"
                Write-Host "Solution Nonce: $($status.solution_nonce)"
                Write-Host "Message: $($status.message)"
                break
            }
        }
        catch {
            $errorMessage = Get-ErrorResponse $_
            Write-Host "`nError getting mining status: $errorMessage"
            Start-Sleep -Seconds 5  # Wait before retrying
        }
        Start-Sleep -Milliseconds 500
    }
}
catch {
    Write-Host "Error: $_" -ForegroundColor Red
    exit 1
}
