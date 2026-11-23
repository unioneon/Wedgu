# Wireless Battery · Windows Style 1.0.4.4

Первый публичный релиз независимого форка **[krisgfx9/Wireless-Device-Battery-Monitor](https://github.com/krisgfx9/Wireless-Device-Battery-Monitor)**, основанный на оригинальной **v1.0.4**. Авторство исходного приложения и его основных драйверов принадлежит оригинальному проекту; лицензия GPL-3.0-or-later сохранена.

## Что сделано

- Более спокойные цвета панели в светлой и тёмной теме и сглаживание мелких иконок/текста.
- Новая векторная иконка геймпада: округлый корпус, крестовина из четырёх штрихов и одно крупное кольцо.
- Исправлен ложный статус сна мыши после пропущенного ответа о батарее; последнее значение отмечается как `last known`.
- Добавлен протокол Lofree Flow Lite84 через приёмник 2,4 ГГц: реальный заряд, зарядка и напряжение.
- Добавлены запросы AJAZZ AK820 Pro, корректное имя и значок по Bluetooth, объединение записей разных подключений.
- Добавлены регрессионные тесты, диагностические утилиты и документация на русском и английском.

## Что проверено

Lofree Flow Lite84 через 2,4 ГГц, AJAZZ AK820 Pro через Bluetooth, Logitech MX Master 3S, Sony WH-1000XM4 и Xbox Wireless Controller.

**Ограничение AJAZZ:** проверенный радиоприёмник не возвращает достоверный процент. Через USB проверенный экземпляр выдаёт служебный маркер, который отображается как `USB`. Для реального процента используйте Bluetooth. Полная поддержка заряда AJAZZ через 2,4 ГГц в этом релизе не заявляется.

## Скачать и запустить

- **Setup.exe** — установщик Windows x64.
- **portable.zip** — распаковать целиком и запустить `WirelessBattery.exe`.
- **headsetcontrol-4.3.0.tar.gz** — соответствующие исходники включённого HeadsetControl.
- **SHA256SUMS.txt** — контрольные суммы файлов выпуска.

Перед запуском новой сборки закройте старую через **Exit**. Для строки с иконками выберите **Show on the taskbar (icon + percentage)**. Если Lofree спит, нажмите на ней несколько клавиш и выберите **Refresh now**.

Исходники этой сборки доступны по тегу релиза, в стандартных архивах **Source code**. Полный перечень изменений — в [CHANGELOG](https://github.com/unioneon/Wedgu/blob/windows-style/CHANGELOG.md).

---

**English:** Independent fork of krisgfx9's v1.0.4, with softer taskbar rendering, a clearer vector controller icon, corrected cached/sleep presentation, Lofree Flow Lite84 receiver support and AJAZZ Bluetooth identity/deduplication. The tested AJAZZ receiver still supplies no usable battery percentage; use Bluetooth. Original attribution and GPL-3.0-or-later licensing are retained.
