# XS 2.0 Blog

This project is the production-oriented XS 2.0 reference application. It uses one
resident xlang handler generation, a bounded four-instance pool, SQLite WAL, safe
Markdown rendering, PBKDF2 password hashes, hashed session tokens and CSRF checks.

## First setup

Set `XS_BLOG_ADMIN_PASSWORD` to a password containing at least 12 bytes. The optional
`XS_BLOG_ADMIN_USER` defaults to `admin`, and `XS_BLOG_DB` defaults to
`projects/xs/blog/data/blog.db`.

```powershell
$env:XS_BLOG_ADMIN_PASSWORD = "replace-with-a-strong-password"
xl run projects/xs/blog/setup.xl
```

## Run

```powershell
xl run projects/xs/xs.xl -- --config projects/xs/blog/xs.json
```

Open `http://127.0.0.1:18090/`. The administration interface is at `/admin`.

For HTTPS, set `tls`, `certFile` and `keyFile` on the HTTP service. The same handler,
virtual-host, reload, queue and graceful-shutdown rules used by XS 2.0 apply here.

## Tests

```powershell
xl run projects/xs/blog/tests/security_test.xl
xl run projects/xs/blog/tests/schema_test.xl
xl run projects/xs/blog/tests/repository_test.xl
powershell -ExecutionPolicy Bypass -File projects/xs/blog/tests/smoke.ps1
```

No default administrator password is stored in source. If the database has no
administrator and the setup environment variable is absent, the public site still
starts and the handler writes a warning to the XS log.

