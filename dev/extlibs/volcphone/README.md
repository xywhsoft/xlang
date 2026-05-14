# x.volc.phone Volcano Engine cloud phone extension

Status: V1 implemented
Design directory: `dev/extlibs/volcphone`
Target package set: `x.volc.core`, `x.volc.sts`, `x.volc.phone`

This directory records the design and delivery plan for the official xlang
Volcano Engine cloud phone extension.

The extension must cover two access paths:

- Server-side OpenAPI control through `open.volcengineapi.com`.
- Temporary STS credentials for vePhoneSDK projection and interactive clients.

The live packages should be created under:

```text
demo5/release/libs/x.volc.core/
demo5/release/libs/x.volc.sts/
demo5/release/libs/x.volc.phone/
```

## Documents

- [x.volc.phone-v1-spec.md](x.volc.phone-v1-spec.md): V1 package boundary,
  signing requirements, STS support, cloud phone API surface, and acceptance
  scope.
- [x.volc.phone-implementation-tracker.md](x.volc.phone-implementation-tracker.md):
  phased implementation checklist.

## Local References

- `C:\Users\gaoweixiong\Desktop\record\sts.py`: working STS AssumeRole
  reference implementation.
- `E:\workspace\llmrunner\CloudPhone.py`: coarse cloud phone wrapper used by a
  real application. This is a coverage reference, not the target API shape.
- `E:\workspace\llmrunner\agentv5\cloud_phone.py`: copied wrapper used by the
  v5 agent path.

## Design Rules

- Do not put user credentials in examples, package metadata, or tests.
- Reuse xrt HTTP, JSON, URL, time, env, and crypto primitives where possible.
- If an xrt primitive cannot express the required byte-level signing behavior,
  fix or extend the primitive before adding product-specific workarounds.
- Keep Volcano OpenAPI signing generic in `x.volc.core`; cloud phone APIs must
  call that layer instead of rebuilding signing logic.
- STS belongs to the official feature set because vePhoneSDK projection depends
  on temporary credentials in real deployments.

## Live API Usage

The typed client API is intended for normal use:

```xl
auto c = phone.client(
	env.read("VOLC_ACCESS_KEY"),
	env.read("VOLC_SECRET_KEY"),
	env.read("VOLC_PHONE_PRODUCT_ID")
)

auto pods = c.list_pods()
```

The single-argument spec APIs remain available for raw calls, diagnostics, and
cases where a user wants to construct the exact OpenAPI payload:

```xl
auto spec = "action=ListPod\nversion=2023-10-30\naccess_key=" + ak +
	"\nsecret_key=" + sk +
	"\nsession_token=" + token +
	"\nbody={\"ProductId\":\"" + product_id + "\"}" +
	"\ntimeout_ms=30000"

auto resp = phone.call_spec(spec)
```

STS uses the same pattern:

```xl
auto spec = "access_key=" + ak +
	"\nsecret_key=" + sk +
	"\nrole_trn=trn:iam::2100310067:role/pingkong_phone" +
	"\nrole_session_name=xlang-client" +
	"\nduration_seconds=3600" +
	"\ntimeout_ms=30000"

auto raw = sts.assume_role_spec(spec)
```

Earlier live testing exposed a runtime bug in `std.env.read()` on Windows: the
function returned a shared static buffer, so consecutive reads could overwrite
previous string variables. That made credential fields appear to be shuffled.
The runtime now returns independent string copies, and a regression example
exists at `demo5/release/examples/native/env_read_lifetime.xl`.

HTTPS calls now use xrt directly with peer verification enabled. The earlier
Windows fallback path was removed after the xrt native TLS receive path was
fixed and real Volcano OpenAPI smoke passed through xrt.
