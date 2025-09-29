#include "mixerwidget.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGroupBox>
#include <QSlider>
#include <QPushButton>
#include <QLabel>
#include <QDebug>

MixerWidget::MixerWidget(QWidget *parent) : QWidget(parent) {
    setLayout(reinterpret_cast<QLayout*>(buildUi()->layout()));
}

QWidget* MixerWidget::buildUi() {
    auto *w = new QWidget(this);
    auto *h = new QHBoxLayout(w);
    h->setSpacing(18);
    h->setContentsMargins(16,16,16,16);

    // ★ 서버 키와 맞춘 세션명: "Guita" / "Drum" / "Piano"
    const QStringList sessions = { "Guita", "Drum", "Piano" };

    for (const QString& s : sessions) {
        auto *strip = makeChannelStrip(s, 75);
        h->addWidget(strip, 0, Qt::AlignTop);
    }
    h->addStretch(1);
    return w;
}

QGroupBox* MixerWidget::makeChannelStrip(const QString& sessionName, int initPercent) {
    auto *gb = new QGroupBox(sessionName, this);
    auto *v = new QVBoxLayout(gb);

    auto *valueLabel = new QLabel(QString::number(initPercent) + "%", gb);
    valueLabel->setAlignment(Qt::AlignHCenter);

    auto *slider = new QSlider(Qt::Vertical, gb);
    slider->setObjectName(QStringLiteral("vol_%1").arg(sessionName));
    slider->setRange(0, 100);
    slider->setPageStep(5);
    slider->setTickPosition(QSlider::TicksBothSides);
    slider->setTickInterval(10);
    slider->setValue(initPercent);

    auto *mute = new QPushButton(tr("Mute"), gb);
    mute->setObjectName(QStringLiteral("mute_%1").arg(sessionName));
    mute->setCheckable(true);

    v->addWidget(valueLabel);
    v->addWidget(slider, 1);
    v->addSpacing(8);
    v->addWidget(mute);

    m_volSliders.insert(sessionName, slider);
    m_muteButtons.insert(sessionName, mute);
    m_valueLabels.insert(sessionName, valueLabel);

    connect(slider, &QSlider::valueChanged, this, [this, sessionName](int val){
        if (m_valueLabels.contains(sessionName))
            m_valueLabels[sessionName]->setText(QString::number(val) + "%");
    });
    connect(slider, &QSlider::valueChanged, this, &MixerWidget::onVolumeChanged);
    connect(mute,   &QPushButton::toggled,  this, &MixerWidget::onMuteToggled);

    return gb;
}

void MixerWidget::onVolumeChanged(int value) {
    auto *sl = qobject_cast<QSlider*>(sender());
    if (!sl) return;
    const QString obj = sl->objectName();   // "vol_<Session>"
    const QString session = obj.mid(4);
    // qDebug() << "[UI]" << session << "vol=" << value;
    emit mixerVolume(session, value);
}

void MixerWidget::onMuteToggled(bool checked) {
    auto *btn = qobject_cast<QPushButton*>(sender());
    if (!btn) return;
    const QString obj = btn->objectName();  // "mute_<Session>"
    const QString session = obj.mid(5);

    if (m_volSliders.contains(session))
        m_volSliders[session]->setEnabled(!checked);

    // qDebug() << "[UI]" << session << "mute=" << checked;
    emit mixerMute(session, checked);
}


