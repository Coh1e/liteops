$ErrorActionPreference='Stop'
$native=Split-Path $PSScriptRoot -Parent
. (Join-Path $native 'source-inputs.ps1')
$fixture=Join-Path $PSScriptRoot ('source-proof-'+[Guid]::NewGuid().ToString('N'))
foreach($directory in @('src','include','vendor','licenses','tests/legacy-source')){New-Item -ItemType Directory -Force (Join-Path $fixture $directory) | Out-Null}
$file=Join-Path $fixture 'src/fixture.cpp'
Set-Content -LiteralPath $file 'synthetic source'
$records=@(Get-SourceInputs $fixture | ForEach-Object {[ordered]@{path=[IO.Path]::GetRelativePath($fixture,$_.FullName);sha256=(Get-FileHash -LiteralPath $_.FullName).Hash.ToLowerInvariant()}})
function Reject([scriptblock]$Action,[string]$Name){$rejected=$false;try{& $Action}catch{$rejected=$true};if(!$rejected){throw "Expected rejection: $Name"};$Name}
$checks=@()
Assert-SourceInputs $fixture $records
$checks+='Matching source set accepted'
$extra=Join-Path $fixture 'src/added.cpp';Set-Content -LiteralPath $extra 'synthetic extra source'
$checks+=Reject {Assert-SourceInputs $fixture $records} 'Added source rejected'
Remove-Item -LiteralPath $extra
Remove-Item -LiteralPath $file
$checks+=Reject {Assert-SourceInputs $fixture $records} 'Removed source rejected'
Set-Content -LiteralPath $file 'changed source'
$checks+=Reject {Assert-SourceInputs $fixture $records} 'Changed source rejected'
Set-Content -LiteralPath $file 'synthetic source'
$checks+=Reject {Assert-SourceInputs $fixture @($records[0],$records[0])} 'Duplicate proof entry rejected'
$checks+=Reject {Assert-SourceInputs $fixture @([ordered]@{path='../outside.cpp';sha256='unused'})} 'Outside-root proof entry rejected'
[ordered]@{status='passed';scope='Source input proof acceptance and rejection using synthetic project files; no GUI or component runtime';checks=$checks;fixtureDirectory=$fixture} | ConvertTo-Json -Depth 5 | Set-Content (Join-Path $native 'evidence/source-input-results.json')
Get-Content (Join-Path $native 'evidence/source-input-results.json')
