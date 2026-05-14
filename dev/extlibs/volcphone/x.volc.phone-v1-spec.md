# x.volc.phone V1 Spec

Status: V1 implemented
Date: 2026-05-13

## Goal

Provide a complete xlang extension for Volcano Engine cloud phone operations,
including OpenAPI management, cloud phone automation, recording/screenshot, task
polling, and STS temporary authorization for vePhoneSDK projection.

The V1 design is based on:

- Existing xrt/xlang runtime capabilities in `demo5`.
- `C:\Users\gaoweixiong\Desktop\record\sts.py`.
- `E:\workspace\llmrunner\CloudPhone.py`.
- `E:\workspace\llmrunner\agentv5\cloud_phone.py`.

## Confirmed Runtime Crypto State

`std.crypto` currently exports:

```xl
hmac_sha256(key, text) as string
hmac_sha512(key, text) as string
```

The implementation calls xrt raw digest functions internally:

- `xrtHMAC_SHA256(...)`
- `xrtHMAC_SHA512(...)`

However, the xlang-facing `std.crypto.hmac_sha256` returns a lowercase hex
string. Volcano request signing requires chained HMAC where the output bytes of
one step are the binary key for the next step:

```text
k_date    = HMAC(secret_key bytes, date)
k_region  = HMAC(k_date bytes, region)
k_service = HMAC(k_region bytes, service)
k_signing = HMAC(k_service bytes, "request")
signature = HMAC(k_signing bytes, string_to_sign).hex()
```

V1 adds both a preferred standard-library route and a package-local fallback:

1. Preferred: `std.crypto` byte-safe helpers:
   - `sha256_hex(text) as string`
   - `hmac_sha256_hex(key, text) as string`
   - `hmac_sha256_hex_key(hex_key, text) as string`
2. Native `x.volc.core` signing artifact that uses `xrtSHA256` and
   `xrtHMAC_SHA256` directly for Volcano signing.

Do not try to derive Volcano signing keys by feeding the previous hex digest as
plain UTF-8 text. That produces the wrong signature.

## Package Split

### `x.volc.core`

Generic Volcano OpenAPI client layer.

Responsibilities:

- Credential model:
  - permanent AK/SK
  - optional session token
  - host
  - region
  - service
  - default version
  - content type
- Canonical query encoding with RFC 3986 safe set `-_.~`.
- Canonical request construction.
- `X-Date`, `X-Content-Sha256`, `Authorization`, and optional session-token
  headers.
- JSON request/response helpers.
- ResponseMetadata error normalization.
- Retry/backoff for transient HTTP failures.
- Redacted debug request preview.
- HTTPS transport through xrt with peer verification enabled. The earlier
  Windows close-before-response failure was traced to the native TLS receive
  path mixing port-driven IO with synchronous `recv()`, and is fixed in xrt.
- Stable single-argument spec entry points. Current live-credential paths avoid
  unnecessary wrapper layers by passing newline-separated `key=value` request
  specs into `x.volc.core`.

Suggested API:

```xl
client = core.client({
    access_key: env("VOLC_ACCESS_KEY"),
    secret_key: env("VOLC_SECRET_KEY"),
    session_token: env("VOLC_SESSION_TOKEN"),
    host: "open.volcengineapi.com",
    region: "cn-north-1",
    service: "ACEP",
    version: "2023-10-30",
    content_type: "application/json"
})

resp = client.call("ListPod", {"ProductId": product_id})
```

Current stable live API:

```xl
spec = "action=ListPod\nversion=2023-10-30\naccess_key=" + ak +
    "\nsecret_key=" + sk +
    "\nsession_token=" + token +
    "\nbody={\"ProductId\":\"" + product_id + "\"}" +
    "\ntimeout_ms=30000"

resp = phone.call_spec(spec)
```

Runtime note:

- Early live tests appeared to show multi-argument or class-field shuffling.
  The root cause was `std.env.read()` returning a shared static Windows buffer:
  consecutive environment reads overwrote earlier string variables.
- `std.env.read()`, `std.env.cwd()`, `std.env.temp()`, and `std.env.home()` now
  return independent copies. The typed `phone.client(...)` path has been
  revalidated against real `ListDc`.
- `call_spec(spec)` and `raw_spec(spec)` remain the most explicit low-level
  OpenAPI entry points.

### `x.volc.sts`

STS AssumeRole layer.

Responsibilities:

- Call `sts.volcengineapi.com`.
- Service is `sts`.
- Default region is `cn-north-1`.
- Default version is `2018-01-01`.
- Support `AssumeRole` with:
  - `RoleTrn`
  - `RoleSessionName`
  - `DurationSeconds`
- Return:
  - `AccessKeyId`
  - `SecretAccessKey`
  - `SessionToken`
  - `ExpiredTime`
  - `CurrentTime`
- Provide conversion into `x.volc.core` credentials.

Suggested API:

```xl
sts_client = sts.client({
    access_key: env("VOLC_ACCESS_KEY"),
    secret_key: env("VOLC_SECRET_KEY")
})

cred = sts_client.assume_role({
    account_id: env("VOLC_ACCOUNT_ID"),
    role_name: "pingkong_phone",
    role_session_name: "xlang-client",
    duration_seconds: 3600
})
```

`x.volc.sts` is not optional for the overall extension. vePhoneSDK projection and
interactive client flows require temporary credentials.

### `x.volc.phone`

Cloud phone product API layer.

Responsibilities:

- Use `x.volc.core` for all signed requests.
- Automatically include configured `ProductId` where applicable.
- Provide both raw action calls and typed convenience APIs.
- Provide task polling helpers for async APIs.
- Provide `call_spec(spec)` and `raw_spec(spec)` as the stable credential-bearing
  entry points for live calls.

Suggested API:

```xl
phone_client = phone.client({
    access_key: env("VOLC_ACCESS_KEY"),
    secret_key: env("VOLC_SECRET_KEY"),
    product_id: env("VOLC_PHONE_PRODUCT_ID")
})

pods = phone_client.list_pods()
pod = phone_client.detail_pod(pod_id)
```

Current V1 also exposes body builder helpers such as `create_pod_body`,
`pod_adb_body`, `command_body`, `recording_body`, `file_body`, and
`pull_file_body`. These are intentionally public so users can inspect or log the
exact JSON payload before sending a signed request.

## V1 Cloud Phone API Coverage

The following APIs are required for V1 because they are either used by
`llmrunner` or needed to make the wrapper generally useful.

Resource discovery:

- `ListDc`
- `ListHost`
- `DetailHost`
- `ListPod`
- `DetailPod`

Pod lifecycle:

- `CreatePod`
- `DeletePod`
- `PowerOnPod`
- `PowerOffPod`
- `RebootPod`
- `ResetPod`

Connectivity and command execution:

- `PodAdb`
- `RunCommand`
- `RunSyncCommand`
- `DetailPod` helper for ADB endpoint extraction

Application management:

- `InstallApp`
- `LaunchApp`
- `CloseApp`
- task polling with `GetTaskInfo`

Media:

- `ScreenShot`
- `BatchScreenShot`
- `StartRecording`
- `StopRecording`

File operations:

- `PushFile`
- `PullFile`

Task helpers:

- `get_task_info(task_id)`
- `wait_task(task_id, timeout_ms, poll_interval_ms)`
- result state normalization: `running`, `success`, `failed`, `unknown`

## `llmrunner` Compatibility Acceptance

After implementation, xlang examples must be able to express the current
`llmrunner` cloud phone behavior without Python `CloudPhone.py`:

- Create one or more pods with `ProductId`, `Dc`, `ConfigurationCode`,
  `ResourceType`, `DisplayLayoutId`, bandwidth limits, and `Start`.
- Enable ADB through `PodAdb`.
- Read pod run state through `DetailPod.Online`.
- Read ADB endpoint through `DetailPod.Adb` when `AdbStatus == 1`.
- Install app and wait for `GetTaskInfo.TaskResult == 100`.
- Start recording with API version `2025-05-01`, explicit `RoundId`, duration
  limit, and `IsSavedOnPod`.
- Stop recording and normalize either direct result data or nested download URL
  fields.
- Take screenshot with a generated `RoundId`.
- List hosts, pods, and DCs.
- Read host remaining pod capacity from `DetailHost`.
- Power on, power off, reboot, reset, and delete pods.

## Error Model

Every typed method should return a structured result:

```text
{
  ok: bool,
  request_id: string,
  action: string,
  code: string,
  code_n: int,
  message: string,
  result: any,
  raw: any
}
```

Convenience helpers may return direct values only when they raise or otherwise
preserve the structured error.

## Security Requirements

- Examples must read credentials from environment variables.
- Debug logs must redact access keys, secret keys, session tokens, and
  Authorization headers.
- Do not copy local `config.ini` secrets into repo files.
- STS temporary credentials should expose expiration and a helper to refresh
  before expiry.

## vePhoneSDK Projection Boundary

`x.volc.sts` exposes a projection credential bootstrap JSON with these fields:

- `access_key`
- `secret_key`
- `session_token`
- `expired_time`
- `current_time`
- `product_id`
- `pod_id`

This object is the handoff contract for vePhoneSDK platform bindings. The
OpenAPI packages mint and refresh temporary credentials; platform-specific
vePhoneSDK packages consume the object to initialize projection and interactive
control. Keep platform bindings separate from `x.volc.phone` so server-side
management APIs do not depend on Android/iOS/Web/Windows SDK details.

## Test Requirements

Offline tests:

- HMAC-SHA256 vector from `demo5/release/examples/native/crypto_basic.xl`.
- Volcano canonical query encoding vector.
- Volcano canonical request/string-to-sign/signature vector copied from a fixed
  fixture.
- STS AssumeRole request signing vector.
- ResponseMetadata error normalization fixtures.
- Recording download URL extraction fixtures.

Live smoke tests:

- `ListPod` with real credentials.
- `DetailPod` for a configured pod.
- `PodAdb` enable/disable on a test pod.
- `StartRecording`/`StopRecording` on a test pod when explicitly enabled.
- `AssumeRole` with a test role when explicitly enabled.
