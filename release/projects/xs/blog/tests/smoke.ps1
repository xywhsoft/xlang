param(
    [int]$DurationMs = 12000
)

$ErrorActionPreference = 'Stop'
$releaseDir = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path
$xlPath = Join-Path $releaseDir 'xl.exe'
$xsPath = Join-Path $releaseDir 'projects\xs\xs.exe'
$setupPath = Join-Path $releaseDir 'projects\xs\blog\setup.xl'
$configPath = Join-Path $releaseDir 'projects\xs\blog\xs.json'
$dataDir = Join-Path $releaseDir 'projects\xs\blog\data'
$databasePath = Join-Path $dataDir 'smoke.db'
$baseUrl = 'http://127.0.0.1:18090'
$adminUser = 'smokeadmin'
$adminPassword = 'Smoke-Test-Password-2026!'
$server = $null

function Assert-True([bool]$condition, [string]$message) {
    if (-not $condition) {
        throw $message
    }
}

function Get-Csrf([string]$html) {
    $match = [regex]::Match($html, "name='csrf' value='([^']+)'")
    Assert-True $match.Success 'CSRF token is missing from the administration page'
    return $match.Groups[1].Value
}

function Remove-SmokeDatabase {
    @($databasePath, "$databasePath-wal", "$databasePath-shm") | ForEach-Object {
        if (Test-Path -LiteralPath $_) {
            Remove-Item -LiteralPath $_ -Force
        }
    }
}

function Wait-Server {
    # Blog handler imports the SQLite source package on a cold process.
    # The released xs.exe is used here, but first-run preparation still needs a bounded warm-up window.
    $deadline = [DateTime]::UtcNow.AddSeconds(90)
    while ([DateTime]::UtcNow -lt $deadline) {
        if ($server.HasExited) {
            $stdout = $server.StandardOutput.ReadToEnd()
            $stderr = $server.StandardError.ReadToEnd()
            throw "XS exited before becoming ready (exit $($server.ExitCode))`n$stdout`n$stderr"
        }
        try {
            $response = Invoke-WebRequest -UseBasicParsing -Uri "$baseUrl/healthz" -TimeoutSec 1
            if ($response.StatusCode -eq 200) {
                return
            }
        } catch {
            Start-Sleep -Milliseconds 100
        }
    }
    throw 'XS did not become ready within 20 seconds'
}

function Start-SmokeServer {
    $info = New-Object System.Diagnostics.ProcessStartInfo
    # Verify the released standalone xs.exe without compiling xs.xl during the health check.
    $info.FileName = $xsPath
    $info.WorkingDirectory = Split-Path -Parent $xsPath
    $info.Arguments = "--config `"$configPath`" --duration $DurationMs"
    $info.UseShellExecute = $false
    $info.CreateNoWindow = $true
    $info.RedirectStandardOutput = $true
    $info.RedirectStandardError = $true
    $info.EnvironmentVariables['XS_BLOG_DB'] = $databasePath
    $info.EnvironmentVariables['XS_BLOG_ADMIN_USER'] = $adminUser
    $info.EnvironmentVariables['XS_BLOG_ADMIN_PASSWORD'] = $adminPassword
    $process = New-Object System.Diagnostics.Process
    $process.StartInfo = $info
    Assert-True $process.Start() 'cannot start XS smoke server'
    return $process
}

New-Item -ItemType Directory -Path $dataDir -Force | Out-Null
Remove-SmokeDatabase
$oldDb = $env:XS_BLOG_DB
$oldUser = $env:XS_BLOG_ADMIN_USER
$oldPassword = $env:XS_BLOG_ADMIN_PASSWORD

try {
    $env:XS_BLOG_DB = $databasePath
    $env:XS_BLOG_ADMIN_USER = $adminUser
    $env:XS_BLOG_ADMIN_PASSWORD = $adminPassword
    & $xlPath run $setupPath
    Assert-True ($LASTEXITCODE -eq 0) 'blog setup failed'

    $server = Start-SmokeServer
    Wait-Server

    $health = Invoke-WebRequest -UseBasicParsing -Uri "$baseUrl/healthz"
    Assert-True ($health.Content -eq '{"status":"ok"}') 'health response is invalid'
    Assert-True ($health.Headers['Content-Security-Policy'] -ne $null) 'security headers are missing'
    $css = Invoke-WebRequest -UseBasicParsing -Uri "$baseUrl/assets/blog.css"
    Assert-True ($css.Headers['Content-Type'] -like 'text/css*') 'CSS content type is invalid'

    $anonymous = New-Object Microsoft.PowerShell.Commands.WebRequestSession
    $adminRedirect = Invoke-WebRequest -UseBasicParsing -Uri "$baseUrl/admin" -WebSession $anonymous
    Assert-True ($adminRedirect.BaseResponse.ResponseUri.AbsolutePath -eq '/admin/login') 'anonymous admin access was not redirected'

    $session = New-Object Microsoft.PowerShell.Commands.WebRequestSession
    $admin = Invoke-WebRequest -UseBasicParsing -Uri "$baseUrl/admin/login" -Method Post `
        -ContentType 'application/x-www-form-urlencoded' `
        -Body @{ username = $adminUser; password = $adminPassword } -WebSession $session
    Assert-True ($admin.BaseResponse.ResponseUri.AbsolutePath -eq '/admin') 'administrator login failed'
    Assert-True ($session.Cookies.Count -eq 1) 'administrator session cookie is missing'
    $csrf = Get-Csrf $admin.Content

    try {
        Invoke-WebRequest -UseBasicParsing -Uri "$baseUrl/admin/categories" -Method Post `
            -ContentType 'application/x-www-form-urlencoded' `
            -Body @{ csrf = 'invalid'; id = '0'; name = 'Rejected'; slug = 'rejected'; enabled = '1' } `
            -WebSession $session | Out-Null
        throw 'invalid CSRF request was accepted'
    } catch {
        $status = $_.Exception.Response.StatusCode.value__
        Assert-True ($status -eq 403) 'invalid CSRF request did not return 403'
    }

    $categories = Invoke-WebRequest -UseBasicParsing -Uri "$baseUrl/admin/categories" -Method Post `
        -ContentType 'application/x-www-form-urlencoded' `
        -Body @{
            csrf = $csrf; id = '0'; action = 'save'; name = 'Engineering'; slug = 'engineering'
            description = 'Technical articles'; sortOrder = '10'; enabled = '1'
        } -WebSession $session
    Assert-True $categories.Content.Contains('Category saved.') 'category creation failed'
    $categoryIds = [regex]::Matches($categories.Content, "name='id' value='([1-9][0-9]*)'")
    Assert-True ($categoryIds.Count -ge 1) 'created category id is missing'
    $categoryId = $categoryIds[0].Groups[1].Value

    $article = Invoke-WebRequest -UseBasicParsing -Uri "$baseUrl/admin/article/save" -Method Post `
        -ContentType 'application/x-www-form-urlencoded' `
        -Body @{
            csrf = $csrf; id = '0'; title = 'Production Smoke Article'; slug = 'production-smoke'
            summary = 'Reference summary'; markdown = "# Hello`n`nThis is **safe**.`n`n<script>alert('x')</script>"
            status = 'published'; categoryId = $categoryId
        } -WebSession $session
    Assert-True ($article.BaseResponse.ResponseUri.AbsolutePath -eq '/admin/article/edit') 'article creation failed'

    $publicArticle = Invoke-WebRequest -UseBasicParsing -Uri "$baseUrl/article/production-smoke"
    Assert-True $publicArticle.Content.Contains('Production Smoke Article') 'published article is missing'
    Assert-True (-not $publicArticle.Content.Contains('<script>')) 'unsafe Markdown HTML reached the response'

    $comment = Invoke-WebRequest -UseBasicParsing -Uri "$baseUrl/article/production-smoke/comment" -Method Post `
        -ContentType 'application/x-www-form-urlencoded' `
        -Body @{ author = 'Reader'; email = 'reader@example.test'; body = 'Pending comment' }
    Assert-True ($comment.BaseResponse.ResponseUri.PathAndQuery -eq '/article/production-smoke?submitted=1') 'comment submission failed'
    $publicArticle = Invoke-WebRequest -UseBasicParsing -Uri "$baseUrl/article/production-smoke"
    Assert-True (-not $publicArticle.Content.Contains('Pending comment')) 'pending comment became public'

    $pending = Invoke-WebRequest -UseBasicParsing -Uri "$baseUrl/admin/comments?status=pending" -WebSession $session
    Assert-True $pending.Content.Contains('Pending comment') 'pending comment is missing from administration'
    $commentId = [regex]::Match($pending.Content, "name='id' value='([1-9][0-9]*)'").Groups[1].Value
    Assert-True ($commentId -ne '') 'pending comment id is missing'
    Invoke-WebRequest -UseBasicParsing -Uri "$baseUrl/admin/comment/action" -Method Post `
        -ContentType 'application/x-www-form-urlencoded' `
        -Body @{ csrf = $csrf; id = $commentId; action = 'visible' } -WebSession $session | Out-Null
    $publicArticle = Invoke-WebRequest -UseBasicParsing -Uri "$baseUrl/article/production-smoke"
    Assert-True $publicArticle.Content.Contains('Pending comment') 'approved comment is not public'

    Invoke-WebRequest -UseBasicParsing -Uri "$baseUrl/guestbook" -Method Post `
        -ContentType 'application/x-www-form-urlencoded' `
        -Body @{ author = 'Visitor'; email = 'visitor@example.test'; body = 'Guestbook pending' } | Out-Null
    $pending = Invoke-WebRequest -UseBasicParsing -Uri "$baseUrl/admin/comments?status=pending" -WebSession $session
    Assert-True $pending.Content.Contains('Guestbook pending') 'pending guestbook message is missing'
    $ids = [regex]::Matches($pending.Content, "name='id' value='([1-9][0-9]*)'") | ForEach-Object { [int]$_.Groups[1].Value }
    $guestbookId = ($ids | Measure-Object -Maximum).Maximum
    Invoke-WebRequest -UseBasicParsing -Uri "$baseUrl/admin/comment/action" -Method Post `
        -ContentType 'application/x-www-form-urlencoded' `
        -Body @{ csrf = $csrf; id = $guestbookId.ToString(); action = 'visible' } -WebSession $session | Out-Null
    $guestbook = Invoke-WebRequest -UseBasicParsing -Uri "$baseUrl/guestbook"
    Assert-True $guestbook.Content.Contains('Guestbook pending') 'approved guestbook message is not public'

    $settings = Invoke-WebRequest -UseBasicParsing -Uri "$baseUrl/admin/settings" -Method Post `
        -ContentType 'application/x-www-form-urlencoded' `
        -Body @{
            csrf = $csrf; title = 'XS Production Blog'; subtitle = 'Built with xlang'
            description = 'Production smoke configuration'; postsPerPage = '7'; moderation = '1'
        } -WebSession $session
    Assert-True $settings.Content.Contains('Settings saved.') 'site settings update failed'
    $sitePage = Invoke-WebRequest -UseBasicParsing -Uri $baseUrl
    Assert-True $sitePage.Content.Contains('XS Production Blog') 'updated site title is not public'

    Assert-True ((Invoke-WebRequest -UseBasicParsing -Uri "$baseUrl/rss.xml").StatusCode -eq 200) 'RSS endpoint failed'
    Assert-True ((Invoke-WebRequest -UseBasicParsing -Uri "$baseUrl/sitemap.xml").StatusCode -eq 200) 'sitemap endpoint failed'
    Assert-True ((Invoke-WebRequest -UseBasicParsing -Uri "$baseUrl/robots.txt").StatusCode -eq 200) 'robots endpoint failed'

    $logout = Invoke-WebRequest -UseBasicParsing -Uri "$baseUrl/admin/logout" -Method Post `
        -ContentType 'application/x-www-form-urlencoded' -Body @{ csrf = $csrf } -WebSession $session
    Assert-True ($logout.BaseResponse.ResponseUri.AbsolutePath -eq '/admin/login') 'logout failed'

    Assert-True $server.WaitForExit($DurationMs + 10000) 'XS did not stop after its bounded duration'
    $stdout = $server.StandardOutput.ReadToEnd()
    $stderr = $server.StandardError.ReadToEnd()
    Assert-True ($server.ExitCode -eq 0) "XS stopped with exit $($server.ExitCode)`n$stdout`n$stderr"
    Write-Host 'xs2 blog smoke test ok'
} finally {
    if ($server -ne $null -and -not $server.HasExited) {
        $server.Kill()
        $server.WaitForExit()
    }
    if ($server -ne $null) {
        $server.Dispose()
    }
    $env:XS_BLOG_DB = $oldDb
    $env:XS_BLOG_ADMIN_USER = $oldUser
    $env:XS_BLOG_ADMIN_PASSWORD = $oldPassword
    Remove-SmokeDatabase
}
