#ifndef MIXERWIDGET_H
#define MIXERWIDGET_H

#include <QWidget>
#include <QMap>

class QSlider;
class QPushButton;
class QLabel;
class QGroupBox;

class MixerWidget : public QWidget {
    Q_OBJECT
public:
    explicit MixerWidget(QWidget *parent = nullptr);

signals:
    // MainWidget에서 ServerWidget 슬롯에 연결한다.
    void mixerVolume(const QString& session, int volume); // 0~100
    void mixerMute(const QString& session, bool mute);    // true=뮤트

private:
    QMap<QString, QSlider*>     m_volSliders;
    QMap<QString, QPushButton*> m_muteButtons;
    QMap<QString, QLabel*>      m_valueLabels;

    QWidget*   buildUi();
    QGroupBox* makeChannelStrip(const QString& sessionName, int initPercent);

private slots:
    void onVolumeChanged(int value);
    void onMuteToggled(bool checked);
};

#endif // MIXERWIDGET_H
