from __future__ import annotations

import sys
from pathlib import Path

from PySide6.QtCore import QLibraryInfo, QLocale, QTranslator
from PySide6.QtGui import QIcon
from PySide6.QtWidgets import QApplication

from vision_control.main_window import MainWindow
from vision_control.styles import APP_STYLE

APP_NAME = "ColorTool_v1.0"


def resource_path(*parts: str) -> Path:
    base_path = Path(getattr(sys, "_MEIPASS", Path(__file__).resolve().parent))
    target = base_path.joinpath(*parts)
    if target.exists():
        return target
    # Fallback in case base_path was expected one level up
    alt = Path(__file__).resolve().parents[1].joinpath(*parts)
    if alt.exists():
        return alt
    return target


def app_icon() -> QIcon:
    icon = QIcon()
    for filename in ("app_icon.ico", "app_icon.png", "app_icon.svg"):
        path = resource_path("vision_control", "assets", filename)
        if path.is_file():
            icon.addFile(str(path))
    return icon


def set_windows_app_id() -> None:
    if sys.platform != "win32":
        return
    try:
        import ctypes

        ctypes.windll.shell32.SetCurrentProcessExplicitAppUserModelID(APP_NAME)
    except Exception:
        return


def install_chinese_translations(app: QApplication) -> None:
    QLocale.setDefault(QLocale(QLocale.Language.Chinese, QLocale.Country.China))
    translations_path = Path(QLibraryInfo.path(QLibraryInfo.LibraryPath.TranslationsPath))
    translators = []
    for name in ("qtbase_zh_CN", "qt_zh_CN"):
        translator = QTranslator(app)
        if translator.load(str(translations_path / f"{name}.qm")):
            app.installTranslator(translator)
            translators.append(translator)
    app._qt_translators = translators


def setup_exception_logging() -> None:
    import traceback

    def handle_exception(exc_type, exc_value, exc_traceback):
        if issubclass(exc_type, KeyboardInterrupt):
            sys.__excepthook__(exc_type, exc_value, exc_traceback)
            return
        log_file = Path(__file__).resolve().parent / "crash.log"
        try:
            with open(log_file, "a", encoding="utf-8") as f:
                f.write("".join(traceback.format_exception(exc_type, exc_value, exc_traceback)))
        except Exception:
            pass
        sys.__excepthook__(exc_type, exc_value, exc_traceback)

    sys.excepthook = handle_exception


def main() -> int:
    setup_exception_logging()
    set_windows_app_id()
    app = QApplication(sys.argv)
    app.setApplicationName(APP_NAME)
    app.setApplicationDisplayName(APP_NAME)
    icon = app_icon()
    app.setWindowIcon(icon)
    install_chinese_translations(app)
    app.setStyleSheet(APP_STYLE)
    window = MainWindow()
    window.setWindowIcon(icon)
    window.show()
    return app.exec()


if __name__ == "__main__":
    raise SystemExit(main())
