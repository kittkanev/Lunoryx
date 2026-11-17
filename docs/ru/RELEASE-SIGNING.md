# Необязательная подпись издателя — подготовка CI

Приложение остаётся офлайн. Подпись/нотаризация — необязательная **ручная операция GitHub Actions**; аккаунтов, сетевого клиента и credentials в редакторе/игре нет. Сертификаты издателя при реализации не предоставлены. Настоящий запуск Authenticode/Developer ID/notarization не заявляется проверенным.

1. Завершить `Build and tests` точного текущего коммита `main`.
2. При необходимости настроить credentials через **GitHub Secrets** репозитория, не через чат, исходники, проекты, артефакты или поле HMAC-ключа.
3. Вручную запустить `Prepare optional platform signatures` (`signing.yml`), указав успешный `source_run`. Другой коммит/ветка и неуспешные jobs отклоняются.
4. Публиковать через `release.yml`, передав исходный `source_run` и необязательный `signing_run`. Gate требует успешный signing-run того же коммита и provenance того же исходного прогона. Пустой `signing_run` оставляет обычный unsigned/ad-hoc путь.

Secrets Windows:

- `LUNORYX_WINDOWS_PFX_BASE64`: base64 PFX издателя с одной private signing identity.
- `LUNORYX_WINDOWS_PFX_PASSWORD`: его непустой пароль.
- `LUNORYX_WINDOWS_TIMESTAMP_URL`: HTTPS timestamp endpoint.

Windows job использует SDK signtool, подписывает собственные бинарники редактора/worker, проверяет Authenticode policy, identity и timestamp, затем удаляет импортированный сертификат/ключ. Бинарники Microsoft/third-party не переподписываются от имени Lunoryx. Временные credentials не входят в артефакты.

Secrets macOS:

- `LUNORYX_MACOS_P12_BASE64`, `LUNORYX_MACOS_P12_PASSWORD`.
- `LUNORYX_MACOS_SIGN_IDENTITY`: точная identity `Developer ID Application: ...`.
- `LUNORYX_NOTARY_KEY_BASE64`, `LUNORYX_NOTARY_KEY_ID`, `LUNORYX_NOTARY_ISSUER_ID`: Apple notary API credentials.

Mac job создаёт временный keychain, подписывает вложенный Mach-O-код и bundle, отправляет запрос нотарификации только из CI, требует `Accepted`, закрепляет/проверяет ticket и выполняет Gatekeeper assessment. JIT/native-library entitlements получает только доверенный C#-worker, сохраняя его прежний full-OS-trust контракт; редактор их не получает. После job keychain/credentials удаляются. Положительные заявления об издателе требуют настоящего нативного запуска с сертификатами.

Без credentials статус — `unsigned` в Windows и `ad_hoc` в macOS, `publisher_verified=false`. Неполный набор — ошибка; одна готовность конфигурации означает лишь `configured_unverified`. `BUILD-SIGNING.json` хранит фактический результат, `SIGNING-PROVENANCE.json` связывает его с исходниками/run, SBOM/checksums пересчитываются после подписи/stapling. Отчёт отвергает неподтверждённые/неполные положительные metadata.

В финальном релизе по-прежнему четыре вложения: Windows ZIP, universal macOS ZIP, Verification.mp4, Audit-report.html. Signing metadata, лицензии, SBOM и checksums — внутри ZIP. Signing workflow только готовит артефакты и не публикует релиз автоматически. HMAC игровых паков — отдельная проверка, не замена подписи издателя.

Build CI выполняет Windows-ветку без credentials и проверяет ad-hoc universal Mac bundle. Policy-тесты покрывают отсутствующую, неполную и полную конфигурацию и отклоняют неподтверждённые положительные metadata. Это не проверка настоящего импорта сертификата, timestamp, Developer ID, нотарификации или Gatekeeper. Для созданных игр нужна собственная подпись после упаковки: сертификат редактора не подписывает изменённый EXE.
