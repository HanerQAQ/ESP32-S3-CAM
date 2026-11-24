from pathlib import Path


COMBO_ARROW_PATH = (Path(__file__).resolve().parent / "assets" / "combo_arrow.svg").as_posix()


APP_STYLE = """
/* ==========================================================================
   Modern Industrial Dark Theme for ColorTool — Blue Navy Edition
   Palette: Deep Navy (#181f30), Slate Navy (#1f2638), Elevated (#283048)
   Accent: Precision Orange (#ff7a00), Cyber Blue (#388bfd), Emerald (#238636)
   ========================================================================== */

* {
    font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", "PingFang SC", "Microsoft YaHei UI", "Helvetica Neue", Arial, sans-serif;
    font-size: 13px;
    color: #f0f6fc;
}

QMainWindow, QWidget#Root {
    background-color: #181f30;
}

/* Tooltips */
QToolTip {
    color: #f0f6fc;
    background-color: #283048;
    border: 1px solid #3d4e6a;
    border-radius: 6px;
    padding: 6px 10px;
    font-size: 12px;
}

/* Base Card Panels */
QFrame[panel="true"] {
    background-color: #1f2638;
    border: 1px solid #2b3450;
    border-radius: 10px;
}

/* Inner Sub-Card */
QFrame[innerCard="true"], QFrame[subPanel="true"] {
    background-color: #283048;
    border: 1px solid #333f5a;
    border-radius: 8px;
}

/* Image Preview Canvases */
QFrame[previewCanvas="true"] {
    background-color: #1c2236;
    border: 1px solid #283048;
    border-radius: 10px;
}

/* Section Header Titles */
QLabel[sectionTitle="true"] {
    color: #f0f6fc;
    font-size: 16px;
    font-weight: 600;
    letter-spacing: 0.3px;
}

/* Active Color Subtitle */
QLabel[activeColorTitle="true"] {
    color: #f0f6fc;
    font-size: 15px;
    font-weight: 600;
}

/* Compact Labels (HSV line titles, min/max) */
QLabel[compactLabel="true"] {
    color: #8b97b0;
    font-size: 12px;
    font-weight: 500;
}

QLabel[connectionHint="true"] {
    color: #f85149;
    font-size: 12px;
}

/* Action Buttons (General) */
QPushButton {
    color: #f0f6fc;
    background-color: #253048;
    border: 1px solid #334060;
    border-radius: 8px;
    padding: 0 12px;
    font-size: 13px;
    font-weight: 500;
}

QPushButton:hover {
    color: #ffffff;
    background-color: #2d3c58;
    border-color: #435578;
}

QPushButton:pressed {
    color: #e6edf3;
    background-color: #1a2035;
    border-color: #ff7a00;
}

QPushButton:disabled {
    color: #484f58;
    background-color: #1c2234;
    border-color: #283048;
}

/* Device Action Buttons (Connect / Capture / Read / Download) */
QPushButton[deviceAction="true"], QPushButton[outlinePrimary="true"] {
    color: #f0f6fc;
    background-color: #253048;
    border: 1px solid #334060;
    border-radius: 8px;
    font-size: 13px;
    font-weight: 500;
}

QPushButton[deviceAction="true"]:hover, QPushButton[outlinePrimary="true"]:hover {
    color: #ffffff;
    background-color: #2d3c58;
    border-color: #435578;
}

QPushButton[deviceAction="true"]:pressed, QPushButton[outlinePrimary="true"]:pressed {
    background-color: #1a2035;
    border-color: #ff7a00;
}

/* Connected State for Connect Button */
QPushButton[deviceAction="true"][connected="true"] {
    color: #3fb950;
    background-color: #12241b;
    border: 1px solid #238636;
}

QPushButton[deviceAction="true"][connected="true"]:hover {
    color: #f85149;
    background-color: #2c1619;
    border: 1px solid #da3633;
}

/* Primary Accent Button (Download to device) */
QPushButton[primaryAccent="true"] {
    color: #ffffff;
    background-color: #ff7a00;
    border: 1px solid #ff7a00;
    border-radius: 8px;
    font-weight: 600;
}

QPushButton[primaryAccent="true"]:hover {
    background-color: #e66e00;
    border-color: #e66e00;
}

QPushButton[primaryAccent="true"]:pressed {
    background-color: #cc6200;
    border-color: #cc6200;
}

/* HSV Quick Pick Button */
QPushButton[hsvPickButton="true"] {
    color: #ff7a00;
    background-color: #253048;
    border: 1px solid #4a3424;
    border-radius: 8px;
    font-size: 13px;
    font-weight: 500;
}

QPushButton[hsvPickButton="true"]:hover {
    color: #ffffff;
    background-color: #ff7a00;
    border-color: #ff7a00;
}

QPushButton[hsvPickButton="true"]:pressed {
    color: #ffffff;
    background-color: #cc6200;
    border-color: #cc6200;
}

/* Hue Invert Toggle Button */
QPushButton[invertButton="true"] {
    color: #8b97b0;
    background-color: #253048;
    border: 1px solid #334060;
    border-radius: 8px;
    font-size: 13px;
    font-weight: 500;
}

QPushButton[invertButton="true"]:hover {
    color: #ffffff;
    background-color: #2d3c58;
    border-color: #435578;
}

QPushButton[invertButton="true"]:checked {
    color: #ff9a40;
    background-color: #221508;
    border: 2px solid #ff7a00;
    font-weight: 700;
    padding: 0 11px;
}

QPushButton[invertButton="true"]:checked:hover {
    background-color: #2a1b08;
    border-color: #ff9a40;
}

/* ComboBox Styling */
QComboBox {
    color: #f0f6fc;
    background-color: #283048;
    border: 1px solid #333f5a;
    border-radius: 8px;
    padding: 4px 12px;
    font-size: 13px;
}

QComboBox:hover {
    background-color: #2d3c58;
    border-color: #435578;
}

QComboBox:focus {
    border-color: #388bfd;
}

QComboBox::drop-down {
    subcontrol-origin: padding;
    subcontrol-position: top right;
    width: 24px;
    border-left: none;
}

QComboBox::down-arrow {
    image: url("__COMBO_ARROW_PATH__");
    width: 10px;
    height: 10px;
}

QComboBox QAbstractItemView {
    color: #f0f6fc;
    background-color: #1f2a40;
    border: 1px solid #303d58;
    border-radius: 8px;
    padding: 4px;
    selection-background-color: #30405e;
    selection-color: #ffffff;
    outline: none;
}

QComboBox QAbstractItemView::item {
    height: 30px;
    padding: 4px 8px;
    border-radius: 5px;
}

QComboBox QAbstractItemView::item:hover {
    background-color: #2d3c58;
    color: #ffffff;
}

/* SpinBox Styling */
QSpinBox[hsvValue="true"] {
    color: #ffffff;
    background-color: #1f2a40;
    border: 1px solid #333f5a;
    border-radius: 6px;
    padding: 2px 4px;
    font-size: 13px;
    font-weight: 600;
    selection-background-color: #ff7a00;
}

QSpinBox[hsvValue="true"]:hover {
    background-color: #283048;
    border-color: #435578;
}

QSpinBox[hsvValue="true"]:focus {
    border-color: #ff7a00;
    background-color: #23304a;
}

QSpinBox[hsvValue="true"] QLineEdit {
    color: #ffffff;
    background: transparent;
    border: none;
    font-size: 13px;
    font-weight: 600;
    selection-background-color: #ff7a00;
    selection-color: #ffffff;
}

QSpinBox[hsvValue="true"]::up-button,
QSpinBox[hsvValue="true"]::down-button {
    width: 0;
    height: 0;
    border: none;
}

/* ScrollBars */
QScrollBar:vertical {
    background: transparent;
    width: 6px;
    margin: 0;
}

QScrollBar::handle:vertical {
    background: #334060;
    border-radius: 3px;
    min-height: 24px;
}

QScrollBar::handle:vertical:hover {
    background: #435578;
}

QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
    height: 0;
}

/* RadioButtons & CheckBoxes */
QRadioButton, QCheckBox {
    color: #f0f6fc;
    font-size: 13px;
    spacing: 8px;
}

QCheckBox::indicator, QRadioButton::indicator {
    width: 16px;
    height: 16px;
    border-radius: 4px;
    border: 1px solid #3d4e6a;
    background-color: #1a2035;
}

QRadioButton::indicator {
    border-radius: 8px;
}

QCheckBox::indicator:hover, QRadioButton::indicator:hover {
    border-color: #ff7a00;
}

QCheckBox::indicator:checked, QRadioButton::indicator:checked {
    background-color: #ff7a00;
    border-color: #ff7a00;
}

/* Connected Status Dot */
QLabel[connectedDot="true"] {
    background-color: #484f58;
    border-radius: 5px;
}

QLabel[connectedDot="true"][online="true"] {
    background-color: #2ea043;
}

/* Dialog Shell */
QFrame[dialogShell="true"] {
    background-color: #1f2638;
    border: 1px solid #334060;
    border-radius: 10px;
}

QLabel[dialogTitle="true"] {
    color: #f0f6fc;
    font-size: 16px;
    font-weight: 600;
}

QFrame[modeColorPanel="true"] {
    background-color: #283048;
    border: 1px solid #333f5a;
    border-radius: 8px;
}
""".replace("__COMBO_ARROW_PATH__", COMBO_ARROW_PATH)
