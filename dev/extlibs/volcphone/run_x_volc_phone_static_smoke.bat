@echo off
setlocal
cd /d %~dp0\..\..\..

set XL=demo5\release\xl.exe
if not exist "%XL%" (
	echo Missing %XL%. Build demo5 first.
	exit /b 1
)

"%XL%" run demo5\release\examples\xle_official_cloud\volc_core_static.xl || exit /b 1
"%XL%" run demo5\release\examples\xle_official_cloud\volc_sts_static.xl || exit /b 1
"%XL%" run demo5\release\examples\xle_official_cloud\volc_phone_static.xl || exit /b 1

echo x.volc.phone static smoke passed.
