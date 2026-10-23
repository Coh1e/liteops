function Get-SourceInputs([string]$Root){
 $files=@()
 foreach($directory in @('src','include','vendor','licenses','tests/legacy-source')){
  $files+=Get-ChildItem -LiteralPath (Join-Path $Root $directory) -File -Recurse
 }
 $files+=Get-ChildItem -LiteralPath $Root -File | Where-Object {$_.Extension -in '.md','.ps1'}
 $files+=Get-ChildItem -LiteralPath (Join-Path $Root 'tests') -File | Where-Object {$_.Extension -in '.ps1','.py','.cs','.txt'}
 $files | Sort-Object FullName -Unique
}

function Assert-SourceInputs([string]$Root,$Records){
 $boundary=[IO.Path]::GetFullPath($Root).TrimEnd('\')+'\'
 $expected=@{}
 foreach($record in $Records){
  $path=[IO.Path]::GetFullPath((Join-Path $Root $record.path))
  if(!$path.StartsWith($boundary,[StringComparison]::OrdinalIgnoreCase) -or $expected.ContainsKey($path)){throw "Invalid or duplicate verified input: $($record.path)"}
  $expected[$path]=$record.sha256
 }
 $current=@(Get-SourceInputs $Root)
 if($current.Count -ne $expected.Count){throw 'Source input set changed since verification.'}
 foreach($file in $current){
  if(!$expected.ContainsKey($file.FullName) -or (Get-FileHash -LiteralPath $file.FullName).Hash.ToLowerInvariant() -ne $expected[$file.FullName]){throw "Unverified or changed source input: $($file.FullName)"}
 }
}
