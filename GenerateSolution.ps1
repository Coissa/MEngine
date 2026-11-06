$ErrorActionPreference = 'Stop'

Push-Location $PSScriptRoot
try {
    cmake --preset windows-vs2026
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

    $generatedSolution = Join-Path $PSScriptRoot 'out/visual-studio/MEngine.slnx'
    [xml]$solution = Get-Content -LiteralPath $generatedSolution -Raw
    foreach ($project in $solution.SelectNodes('//Project[@Path]')) {
        $project.SetAttribute('Path', 'out/visual-studio/' + $project.GetAttribute('Path').Replace('\', '/'))
    }
    foreach ($dependency in $solution.SelectNodes('//BuildDependency[@Project]')) {
        $dependency.SetAttribute('Project', 'out/visual-studio/' + $dependency.GetAttribute('Project').Replace('\', '/'))
    }

    $settings = [System.Xml.XmlWriterSettings]::new()
    $settings.Indent = $true
    $settings.OmitXmlDeclaration = $true
    $settings.Encoding = [System.Text.UTF8Encoding]::new($false)
    $writer = [System.Xml.XmlWriter]::Create((Join-Path $PSScriptRoot 'MEngine.slnx'), $settings)
    try { $solution.Save($writer) } finally { $writer.Dispose() }

    Write-Host 'Generated MEngine.slnx. Open it in Visual Studio 2026.'
}
finally {
    Pop-Location
}
