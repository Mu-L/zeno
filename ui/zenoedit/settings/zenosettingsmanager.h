#ifndef __ZENOSETTINGS_MANAGR__
#define __ZENOSETTINGS_MANAGER__

#include <QVariant>
#include <QObject>
#include <QSettings>
#include "settings/zsettings.h"

struct ShortCutInfo {
    QString key;
    QString desc;
    QString shortcut;
};
Q_DECLARE_METATYPE(ShortCutInfo)

class ZenoSettingsManager : public QObject
{
    Q_OBJECT
public:
    static ZenoSettingsManager& GetInstance();
    void setValue(const QString& name, const QVariant& value);
    QVariant getValue(const QString& zsName) const;

    const int getShortCut(const QString &key);
    void setShortCut(const QString &key, const QString &value);

  signals:
    void valueChanged(QString zsName);

private:
    void initShortCutInfos();
    ShortCutInfo& getShortCutInfo(const QString &key);

  private:
    ZenoSettingsManager(QObject *parent = nullptr);
    QVector<ShortCutInfo> m_shortCutInfos;
};


#endif