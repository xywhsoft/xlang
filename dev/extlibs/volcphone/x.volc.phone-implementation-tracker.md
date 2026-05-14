# x.volc.phone Implementation Tracker

Status: V1 implemented with native x.volc.core signing and xrt HTTPS transport
Date: 2026-05-13

## Phase 0 - Runtime Crypto Gap

- [x] Confirm xrt has raw `xrtSHA256` and `xrtHMAC_SHA256`.
- [x] Confirm `std.crypto.hmac_sha256` exists.
- [x] Confirm `std.crypto.hmac_sha256` returns lowercase hex text.
- [x] Add byte-safe signing support for chained HMAC:
  - [x] preferred: extend `std.crypto`
  - [x] fallback: native `x.volc.core` signing artifact
- [x] Add offline signing vector tests.

Acceptance:

- Volcano signing key derivation uses raw HMAC bytes between derivation steps.
- Existing `crypto_basic.xl` remains compatible.

## Phase 1 - `x.volc.core`

- [x] Create `demo5/release/libs/x.volc.core/package.json`.
- [x] Create native implementation `demo5/release/libs/x.volc.core/src/x_volc_core.c`.
- [x] Implement high-level credential object.
- [x] Implement canonical query encoding.
- [x] Implement canonical request.
- [x] Implement string-to-sign.
- [x] Implement Authorization header.
- [x] Implement signed POST JSON call.
- [x] Implement signed GET query call for STS reuse or expose generic method.
- [x] Normalize `ResponseMetadata` errors.
- [x] Add redacted debug preview.
- [x] Verify real HTTPS calls through xrt after fixing the native TLS receive
  path close-before-response failure.
- [x] Add single-argument `*_spec` request entry points to avoid current xlang
  multi-string/class-field argument shuffling in live credential paths.
- [x] Root-cause the apparent argument shuffle: Windows `std.env.read()` reused
  one static buffer, so consecutive reads overwrote earlier string variables.
- [x] Fix `std.env.read()`, `std.env.cwd()`, `std.env.temp()`, and
  `std.env.home()` to return independent string copies.
- [x] Add `demo5/release/examples/native/env_read_lifetime.xl` regression.

Acceptance:

- `ListPod` can be called through the stable single-argument spec path.
- Offline fixture signature matches the Python reference behavior in
  `E:\workspace\llmrunner\CloudPhone.py`.
- Real `ListDc`, `ListHost`, and `ListPod` calls pass against
  `E:\workspace\llmrunner\config.ini`.

## Phase 2 - `x.volc.sts`

- [x] Create `demo5/release/libs/x.volc.sts/package.json`.
- [x] Create `demo5/release/libs/x.volc.sts/src/sts.xl`.
- [x] Implement `assume_role`.
- [x] Implement `assume_role_spec` stable single-argument entry point.
- [x] Support `role_trn` direct input.
- [x] Support `account_id + role_name` TRN construction.
- [x] Support `role_session_name`.
- [x] Support `duration_seconds`.
- [x] Return raw AssumeRole response containing temporary AK/SK/session token/expiry.
- [x] Add helper to convert STS credentials into `x.volc.core` credentials.

Acceptance:

- The xlang STS example can replace
  `C:\Users\gaoweixiong\Desktop\record\sts.py` for AssumeRole.
- The returned session token can be passed to vePhoneSDK-side bootstrap code.
- Real STS AssumeRole smoke passes with the role values from
  `C:\Users\gaoweixiong\Desktop\record\sts.py`.

## Phase 3 - `x.volc.phone` V1 Wrapper

- [x] Create `demo5/release/libs/x.volc.phone/package.json`.
- [x] Create `demo5/release/libs/x.volc.phone/src/phone.xl`.
- [x] Implement `client(config)`.
- [x] Implement raw `call(action, body, options)`.
- [x] Implement stable raw `call_spec(spec)` / `raw_spec(spec)`.
- [x] Implement `ListDc`.
- [x] Implement `ListHost`.
- [x] Implement `DetailHost`.
- [x] Implement `ListPod`.
- [x] Implement `DetailPod`.
- [x] Implement `CreatePod` single and batch helpers.
- [x] Implement `DeletePod`.
- [x] Implement `PowerOnPod`.
- [x] Implement `PowerOffPod`.
- [x] Implement `RebootPod`.
- [x] Implement `ResetPod`.
- [x] Implement `PodAdb`.
- [x] Implement `RunCommand`.
- [x] Implement `RunSyncCommand`.
- [x] Implement `InstallApp`.
- [x] Implement `LaunchApp`.
- [x] Implement `CloseApp`.
- [x] Implement `GetTaskInfo`.
- [x] Implement `wait_task`.
- [x] Implement `ScreenShot`.
- [x] Implement `BatchScreenShot`.
- [x] Implement `StartRecording` with version override `2025-05-01`.
- [x] Implement `StopRecording` with version override `2025-05-01`.
- [x] Implement recording download URL extraction.
- [x] Implement `PushFile`.
- [x] Implement `PullFile`.

Acceptance:

- `llmrunner` cloud phone behavior listed in the spec can be reproduced from
  xlang examples without importing Python `CloudPhone.py`.
- The public API is broader and cleaner than `CloudPhone.py`; the Python file is
  only a compatibility coverage reference.
- The typed client path and the single-argument spec path both work for live
  calls. `*_spec` remains useful for diagnostics and exact raw OpenAPI payloads.

## Phase 4 - Examples And Smoke Tests

- [x] Add `demo5/release/examples/xle_official_cloud/volc_phone_list_pods.xl`.
- [x] Add `demo5/release/examples/xle_official_cloud/volc_phone_adb.xl`.
- [x] Add `demo5/release/examples/xle_official_cloud/volc_phone_install_app.xl`.
- [x] Add `demo5/release/examples/xle_official_cloud/volc_phone_recording.xl`.
- [x] Add `demo5/release/examples/xle_official_cloud/volc_sts_assume_role.xl`.
- [x] Add `demo5/release/examples/xle_official_cloud/volc_phone_readonly_live.xl`.
- [x] Add `demo5/release/examples/xle_official_cloud/volc_phone_create_delete_live.xl`.
- [x] Add `dev/extlibs/volcphone/run_x_volc_phone_static_smoke.bat`.
- [x] Add `dev/extlibs/volcphone/run_x_volc_phone_live_smoke.bat`.

Acceptance:

- Static smoke runs without credentials.
- Live smoke requires explicit environment variables and never prints secrets.
- Default live smoke performs only read-only phone calls and STS. Mutating pod
  tests require `VOLC_PHONE_TEST_CREATE=1`.
- Create/delete live smoke reached authenticated CreatePod, but the current
  product returned `InternalError.DownStreamCodeProductConcurrentNotEnough`
  (`limit: 0, current: 1`), so no test pod was created or deleted.

## Phase 5 - vePhoneSDK Integration Boundary

- [x] Define a small credential bootstrap schema for client-side projection:
  - [x] temporary AK
  - [x] temporary SK
  - [x] session token
  - [x] expiry
  - [x] product id
  - [x] pod id
- [x] Document how `x.volc.sts` feeds vePhoneSDK initialization.
- [x] Keep vePhoneSDK platform bindings separate from OpenAPI wrappers.

Acceptance:

- The OpenAPI library can mint temporary credentials for projection clients.
- Platform-specific vePhoneSDK bindings can consume the same credential object
  without depending on cloud phone management APIs.
