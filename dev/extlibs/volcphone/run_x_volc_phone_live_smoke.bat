@echo off
setlocal
cd /d %~dp0\..\..\..

set XL=demo5\release\xl.exe
if not exist "%XL%" (
	echo Missing %XL%. Build demo5 first.
	exit /b 1
)

"%XL%" run demo5\release\examples\xle_official_cloud\volc_phone_list_pods.xl || exit /b 1
"%XL%" run demo5\release\examples\xle_official_cloud\volc_phone_readonly_live.xl || exit /b 1
"%XL%" run demo5\release\examples\xle_official_cloud\volc_sts_assume_role.xl || exit /b 1
"%XL%" run demo5\release\examples\xle_official_cloud\volc_phone_create_delete_live.xl || exit /b 1

echo x.volc.phone live smoke passed or skipped by missing env.
