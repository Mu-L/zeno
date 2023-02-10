#include "zenogvhelper.h"
#include "../../nodesys/nodesys_common.h"
#include "zlayoutbackground.h"
#include "zenoparamwidget.h"
#include "zassert.h"
#include <zenomodel/include/uihelper.h>
#include "zveceditoritem.h"


void ZenoGvHelper::setSizeInfo(QGraphicsItem* item, const SizeInfo& sz)
{
    int type = item->type();
    switch (type)
    {
        case QGraphicsProxyWidget::Type:
        {
            QGraphicsProxyWidget* pItem = qgraphicsitem_cast<QGraphicsProxyWidget*>(item);
            pItem->setGeometry(QRectF(sz.pos, sz.minSize));
            break;
        }
        case QGraphicsWidget::Type:
        {
            QGraphicsWidget* pItem = qgraphicsitem_cast<QGraphicsWidget*>(item);
            pItem->setGeometry(QRectF(sz.pos, sz.minSize));
            break;
        }
        case QGraphicsTextItem::Type:
        {
            QGraphicsTextItem* pItem = qgraphicsitem_cast<QGraphicsTextItem*>(item);
            pItem->setPos(sz.pos);
            pItem->setData(GVKEY_BOUNDING, sz.minSize);
            break;
        }
        case QGraphicsRectItem::Type:
        {
            QGraphicsRectItem* pItem = qgraphicsitem_cast<QGraphicsRectItem*>(item);
            ZASSERT_EXIT(pItem);
            pItem->setRect(QRectF(sz.pos, sz.minSize));
            break;
        }
        case QGraphicsEllipseItem::Type:
        {
            QGraphicsEllipseItem* pItem = qgraphicsitem_cast<QGraphicsEllipseItem*>(item);
            ZASSERT_EXIT(pItem);
            pItem->setRect(QRectF(sz.pos, sz.minSize));
            break;
        }
        default:
        {
            QGraphicsItem* pItem = qgraphicsitem_cast<QGraphicsItem*>(item);
            pItem->setPos(sz.pos);
            pItem->setData(GVKEY_BOUNDING, sz.minSize);
            break;
        }
    }
}

void ZenoGvHelper::setValue(QGraphicsItem* item, PARAM_CONTROL ctrl, const QVariant& value)
{
    if (!item)
        return;
    int type = item->type();
    switch (type)
    {
        case QGraphicsProxyWidget::Type:
        {
            QGraphicsProxyWidget* pItem = qgraphicsitem_cast<QGraphicsProxyWidget*>(item);
            BlockSignalScope scope(pItem);
            if (ZenoParamLineEdit* pLineEdit = qobject_cast<ZenoParamLineEdit*>(pItem))
            {
                if (ctrl == CONTROL_FLOAT)
                    pLineEdit->setText(QString::number(value.toFloat()));
                else
                    pLineEdit->setText(value.toString());
            }
            else if (ZenoParamCheckBox* pCheckbox = qobject_cast<ZenoParamCheckBox*>(pItem))
            {
                pCheckbox->setCheckState(value.toBool() ? Qt::Checked : Qt::Unchecked);
            }
            else if (ZenoParamPathEdit* pathWidget = qobject_cast<ZenoParamPathEdit*>(pItem))
            {
                pathWidget->setPath(value.toString());
            }
            else if (ZenoParamMultilineStr* pMultiStrEdit = qobject_cast<ZenoParamMultilineStr*>(pItem))
            {
                pMultiStrEdit->setText(value.toString());
            }
            else if (ZenoParamPushButton* pBtn = qobject_cast<ZenoParamPushButton*>(pItem))
            {
                //nothing need to be done.
            }
            else if (ZenoVecEditItem* pBtn = qobject_cast<ZenoVecEditItem*>(pItem))
            {
                UI_VECTYPE vec = value.value<UI_VECTYPE>();
                pBtn->setVec(vec);
            }
            else if (ZVecEditorItem* pEditor = qobject_cast<ZVecEditorItem*>(pItem))
            {
                UI_VECTYPE vec = value.value<UI_VECTYPE>();
                pEditor->setVec(vec);
            }
            else if (ZenoParamComboBox* pBtn = qobject_cast<ZenoParamComboBox*>(pItem))
            {
                pBtn->setText(value.toString());
            } 
            else if (ZenoParamSlider *pSlider = qobject_cast<ZenoParamSlider *>(pItem)) 
            {
                pSlider->setValue(value.toInt());
            }
            else if (ZenoParamSpinBoxSlider *pSlider = qobject_cast<ZenoParamSpinBoxSlider *>(pItem)) 
            {
                pSlider->setValue(value.toInt());
            }
            else if (ZenoParamSpinBox*pSpinBox = qobject_cast<ZenoParamSpinBox *>(pItem)) 
            {
                pSpinBox->setValue(value.toInt());
            }
            break;
        }
        case QGraphicsTextItem::Type:
        {
            QGraphicsTextItem* pItem = qgraphicsitem_cast<QGraphicsTextItem*>(item);
            if (ctrl == CONTROL_FLOAT)
                pItem->setPlainText(QString::number(value.toFloat()));
            else
                pItem->setPlainText(value.toString());
            break;
        }
        case QGraphicsWidget::Type:
        {
        }
        default:
            break;
    }
}

void ZenoGvHelper::setCtrlProperties(QGraphicsItem *item,  const QVariant &value) 
{
    if (value.type() == QMetaType::QVariantMap) 
    {
            QGraphicsProxyWidget *pItem = qgraphicsitem_cast<QGraphicsProxyWidget *>(item);
            BlockSignalScope scope(pItem);

            if (ZenoParamComboBox *combBox = qobject_cast<ZenoParamComboBox *>(pItem)) 
            {
                const QVariantMap &map = value.toMap();
                if (map.contains("items")) {
                    combBox->setItems(map["items"].toStringList());
                }
            } 
            ZenoParamSlider *pSlider = qobject_cast<ZenoParamSlider *>(pItem);
            ZenoParamSpinBoxSlider *pSpinBoxSlider = qobject_cast<ZenoParamSpinBoxSlider *>(pItem);
            if (pSlider || pSpinBoxSlider) 
            {
                const QVariantMap &map = value.toMap();
                SLIDER_INFO info;
                if (map.contains("min")) 
                {
                    info.min = map["min"].toInt();
                }
                if (map.contains("max")) 
                {
                    info.max = map["max"].toInt();
                }
                if (map.contains("step")) 
                {
                    info.step = map["step"].toInt();
                }
                if (pSlider)
                    pSlider->setSliderInfo(info);
                else if (pSpinBoxSlider)
                    pSpinBoxSlider->setSliderInfo(info);
            }
    }
}

QSizeF ZenoGvHelper::sizehintByPolicy(QGraphicsItem* item)
{
    if (!item) return QSizeF();

    QSizeF sizeHint = item->data(GVKEY_SIZEHINT).toSizeF();
    QSizePolicy policy = item->data(GVKEY_SIZEPOLICY).value<QSizePolicy>();
    QRectF br = item->boundingRect();
    QSizeF brSize = item->boundingRect().size();

    qreal w = 0, h = 0;

    if (!sizeHint.isValid())
        return brSize;

    if (policy.horizontalPolicy() == QSizePolicy::Preferred)
        w = br.width();
    else
        w = sizeHint.width();

    if (policy.verticalPolicy() == QSizePolicy::Preferred)
        h = br.height();
    else
        h = sizeHint.height();

    return QSizeF(w, h);
}