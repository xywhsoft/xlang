@echo off
setlocal
cd /d %~dp0\..\..\..

call tools\xl6_toolchain_env.bat
if errorlevel 1 exit /b 1

if not exist build\test\examples_apps mkdir build\test\examples_apps

echo Checking application examples...
for %%f in (
    asset_manifest.xl
    cli_args.xl
    config_loader.xl
    csv_sales_report.xl
    file_backup.xl
    log_summary.xl
    static_site_builder.xl
    template_mail_merge.xl
    todo_json_store.xl
    url_batch_builder.xl
) do (
    echo CHECK %%f
    release\xl.exe check "release\examples\apps\%%f"
    if errorlevel 1 exit /b 1
)

echo Running application examples...
for %%f in (
    asset_manifest.xl
    config_loader.xl
    csv_sales_report.xl
    file_backup.xl
    log_summary.xl
    static_site_builder.xl
    template_mail_merge.xl
    todo_json_store.xl
    url_batch_builder.xl
) do (
    echo RUN %%f
    release\xl.exe run "release\examples\apps\%%f" %XL6_XL_DEFAULT_LINK_ARGS%
    if errorlevel 1 exit /b 1
)

echo RUN cli_args.xl
release\xl.exe run release\examples\apps\cli_args.xl %XL6_XL_DEFAULT_LINK_ARGS% -- report --name xlang --limit 3
if errorlevel 1 exit /b 1

echo Application examples ok.
exit /b 0
