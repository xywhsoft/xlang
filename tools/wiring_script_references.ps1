function Get-WiringScriptClosure([string]$Root, [string[]]$Entries) {
    $rootPath = [IO.Path]::GetFullPath($Root).TrimEnd('\','/')
    $python = (Get-Command python -CommandType Application -ErrorAction Stop | Select-Object -First 1).Source
    $parsed = & $python (Join-Path $PSScriptRoot 'wiring_python_literals.py') $rootPath
    if ($LASTEXITCODE) { throw 'Cannot parse Python test-runner references' }
    $pythonStrings = $parsed | ConvertFrom-Json
    $seen = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    $pending = [Collections.Generic.Queue[string]]::new()
    foreach ($entry in $Entries) { $pending.Enqueue((Join-Path $rootPath $entry)) }
    $records = [Collections.Generic.List[object]]::new()
    while ($pending.Count) {
        $path = [IO.Path]::GetFullPath($pending.Dequeue())
        if (!$path.StartsWith($rootPath + [IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase) -or
            !(Test-Path -LiteralPath $path -PathType Leaf) -or !$seen.Add($path)) { continue }
        $text = [IO.File]::ReadAllText($path,[Text.Encoding]::UTF8)
        $values = @()
        if ([IO.Path]::GetExtension($path) -eq '.ps1') {
            $tokens=$null; $errors=$null
            $ast=[Management.Automation.Language.Parser]::ParseInput($text,[ref]$tokens,[ref]$errors)
            if ($errors.Count) { throw "Cannot parse runner: $path" }
            $values=@($ast.FindAll({param($node) $node -is [Management.Automation.Language.StringConstantExpressionAst] -or
                $node -is [Management.Automation.Language.ExpandableStringExpressionAst]},$true) | ForEach-Object Value)
        } elseif ([IO.Path]::GetExtension($path) -eq '.py') {
            $property=$pythonStrings.PSObject.Properties[$path]
            if ($property) { $values=@($property.Value) }
        } else {
            $values=@([regex]::Matches($text,'["'']([^"''\r\n]+)["'']') | ForEach-Object {$_.Groups[1].Value})
            $values+=@([regex]::Matches($text,'(?im)(?:^|[\s"''])(?<path>(?:tools|test|release)[\\/][A-Za-z0-9_./\\-]+\.(?:ps1|bat|py|sh|json|txt))(?=$|[\s"''])') |
                ForEach-Object {$_.Groups['path'].Value})
        }
        $records.Add([pscustomobject]@{Path=$path;Text=$text;Literals=$values})
        foreach ($value in $values) {
            if ($value -notmatch '\.(ps1|bat|py|sh|json|txt)$' -or $value -match '[\r\n]|://|[*?]') { continue }
            $value=$value.Replace('$PSScriptRoot',[IO.Path]::GetDirectoryName($path)).Replace('${PSScriptRoot}',[IO.Path]::GetDirectoryName($path))
            $value=$value.Replace('$Root',$rootPath).Replace('$root',$rootPath).Replace('${Root}',$rootPath).Replace('${root}',$rootPath)
            $value=$value.Replace('$ROOT',$rootPath).Replace('${ROOT}',$rootPath)
            if ($value -match '\$|%') { continue }
            foreach ($parent in @([IO.Path]::GetDirectoryName($path),$rootPath,(Join-Path $rootPath 'tools'))) {
                try {
                    $candidate=[IO.Path]::GetFullPath((Join-Path $parent $value))
                    $relative=$candidate.Substring([Math]::Min($candidate.Length,$rootPath.Length+1)).Replace('\','/')
                    if ($relative -match '^(build/|docs/|release/tmp/)') { continue }
                    if ($candidate.StartsWith($rootPath + [IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase) -and
                        (Test-Path -LiteralPath $candidate -PathType Leaf)) { $pending.Enqueue($candidate) }
                } catch {
                    $cause=$_.Exception.InnerException
                    if ($cause -isnot [ArgumentException] -and $cause -isnot [IO.PathTooLongException] -and
                        $cause -isnot [NotSupportedException]) { throw }
                }
            }
        }
    }
    return $records.ToArray()
}
