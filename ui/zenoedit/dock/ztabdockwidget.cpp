#include "ztabdockwidget.h"
#include <zenoui/comctrl/zdocktabwidget.h>
#include "zenoapplication.h"
#include "../panel/zenodatapanel.h"
#include "panel/zenoproppanel.h"
#include "../panel/zenospreadsheet.h"
#include "../panel/zlogpanel.h"
#include "viewport/viewportwidget.h"
#include "nodesview/zenographseditor.h"
#include <zenoui/comctrl/zlabel.h>
#include "zenomainwindow.h"
#include "zenoapplication.h"
#include <zenomodel/include/graphsmanagment.h>
#include "docktabcontent.h"
#include <zenoui/style/zenostyle.h>
#include <zenoui/comctrl/zicontoolbutton.h>
#include <zenomodel/include/modelrole.h>
#include <zenovis/ObjectsManager.h>


ZTabDockWidget::ZTabDockWidget(ZenoMainWindow* mainWin, Qt::WindowFlags flags)
    : _base(mainWin, flags)
    , m_tabWidget(new ZDockTabWidget)
{
    setFeatures(QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetFloatable);
    setWidget(m_tabWidget);

    connect(this, SIGNAL(maximizeTriggered()), mainWin, SLOT(onMaximumTriggered()));
    connect(this, SIGNAL(splitRequest(bool)), mainWin, SLOT(onSplitDock(bool)));
    connect(this, SIGNAL(closeRequest()), mainWin, SLOT(onCloseDock()));

    setTitleBarWidget(new QWidget(this));
    connect(m_tabWidget, SIGNAL(addClicked()), this, SLOT(onAddTabClicked()));
    connect(m_tabWidget, SIGNAL(layoutBtnClicked()), this, SLOT(onDockOptionsClicked()));
    connect(this, SIGNAL(dockLocationChanged(Qt::DockWidgetArea)),
        mainWin, SLOT(onDockLocationChanged(Qt::DockWidgetArea)));
    connect(m_tabWidget, &ZDockTabWidget::tabClosed, this, [=]() {
        if (m_tabWidget->count() == 0) {
            this->close();
        }
    });
}

ZTabDockWidget::~ZTabDockWidget()
{

}

void ZTabDockWidget::setCurrentWidget(PANEL_TYPE type)
{
    m_debugPanel = type;
    QWidget* wid = createTabWidget(type);
    if (wid)
    {
        int idx = m_tabWidget->addTab(wid, type2Title(type));
        m_tabWidget->setCurrentIndex(idx);
    }
}

int ZTabDockWidget::count() const
{
    return m_tabWidget ? m_tabWidget->count() : 0;
}

QWidget* ZTabDockWidget::widget(int i) const
{
    if (i < 0 || i >= count())
        return nullptr;

    return m_tabWidget->widget(i);
}

DisplayWidget* ZTabDockWidget::getUniqueViewport() const
{
    if (1 == count())
    {
        QWidget* wid = m_tabWidget->widget(0);
        return qobject_cast<DisplayWidget*>(wid);
    }
    else {
        return nullptr;
    }
}

ZenoGraphsEditor* ZTabDockWidget::getAnyEditor() const
{
    for (int i = 0; i < m_tabWidget->count(); i++)
    {
        QWidget* wid = m_tabWidget->widget(0);
        if (DockContent_Editor* pEditor = qobject_cast<DockContent_Editor*>(wid))
        {
            return pEditor->getEditor();
        }
    }
    return nullptr;
}

QWidget* ZTabDockWidget::createTabWidget(PANEL_TYPE type)
{
    ZenoMainWindow* pMainWin = zenoApp->getMainWindow();
    switch (type)
    {
        case PANEL_NODE_PARAMS:
        {
            DockContent_Parameter* wid = new DockContent_Parameter;
            return wid;
        }
        case PANEL_VIEW:
        {
            return new DisplayWidget;
        }
        case PANEL_EDITOR:
        {
            return new DockContent_Editor;
        }
        case PANEL_NODE_DATA:
        {
            return new ZenoSpreadsheet;
        }
        case PANEL_LIGHT:
        {
            return new ZenoLights;
        }
        case PANEL_LOG:
        {
            return new ZPlainLogPanel;
        }
    }
    return nullptr;
}

QString ZTabDockWidget::type2Title(PANEL_TYPE type)
{
    switch (type)
    {
    case PANEL_VIEW:        return tr("View");
    case PANEL_EDITOR:      return tr("Editor");
    case PANEL_NODE_PARAMS: return tr("Parameter");
    case PANEL_NODE_DATA:   return tr("Data");
    case PANEL_LOG:         return tr("Logger");
    case PANEL_LIGHT:       return tr("Light");
    default:
        return "";
    }
}

PANEL_TYPE ZTabDockWidget::title2Type(const QString& title)
{
    PANEL_TYPE type = PANEL_EMPTY;
    if (title == tr("Parameter") || title == "Parameter") {
        type = PANEL_NODE_PARAMS;
    }
    else if (title == tr("View") || title == "View") {
        type = PANEL_VIEW;
    }
    else if (title == tr("Editor") || title == "Editor") {
        type = PANEL_EDITOR;
    }
    else if (title == tr("Data") || title == "Data") {
        type = PANEL_NODE_DATA;
    }
    else if (title == tr("Logger") || title == "Logger") {
        type = PANEL_LOG;
    }
    else if (title == tr("Light")|| title == "Light") {
        type = PANEL_LIGHT;
    }
    return type;
}

void ZTabDockWidget::onNodesSelected(const QModelIndex& subgIdx, const QModelIndexList& nodes, bool select)
{
    for (int i = 0; i < m_tabWidget->count(); i++)
    {
        QWidget* wid = m_tabWidget->widget(i);
        if (DockContent_Parameter* prop = qobject_cast<DockContent_Parameter*>(wid))
        {
            prop->onNodesSelected(subgIdx, nodes, select);
        }
        else if (ZenoSpreadsheet* panel = qobject_cast<ZenoSpreadsheet*>(wid))
        {
            IGraphsModel* pModel = zenoApp->graphsManagment()->currentModel();
            if (select)
            {
                const QModelIndex &idx = nodes[0];
                QString nodeId = idx.data(ROLE_OBJID).toString();
                auto *scene = Zenovis::GetInstance().getSession()->get_scene();
                scene->selected.clear();
                std::string nodeid = nodeId.toStdString();
                for (auto const &[key, ptr] : scene->objectsMan->pairs()) {
                    if (nodeid == key.substr(0, key.find_first_of(':'))) {
                        scene->selected.insert(key);
                    }
                }
                onPrimitiveSelected(scene->selected);
            }
        }
    }
}

void ZTabDockWidget::onPrimitiveSelected(const std::unordered_set<std::string>& primids)
{
    for (int i = 0; i < m_tabWidget->count(); i++)
    {
        QWidget* wid = m_tabWidget->widget(i);
        if (ZenoSpreadsheet* panel = qobject_cast<ZenoSpreadsheet*>(wid))
        {
            if (primids.size() == 1) {
                panel->setPrim(*primids.begin());
            }
            else {
                panel->clear();
            }
        }
    }
}

void ZTabDockWidget::newFrameUpdate()
{
    for (int i = 0; i < m_tabWidget->count(); i++)
    {
        QWidget* wid = m_tabWidget->widget(i);
        if (ZenoLights* panel = qobject_cast<ZenoLights*>(wid))
        {
            panel->updateLights();
        }
    }
}

void ZTabDockWidget::onUpdateViewport(const QString& action)
{
    for (int i = 0; i < m_tabWidget->count(); i++)
    {
        QWidget* wid = m_tabWidget->widget(i);
        if (DisplayWidget* pView = qobject_cast<DisplayWidget*>(wid))
        {
            pView->updateFrame(action);
        }
    }
}

void ZTabDockWidget::onRunFinished()
{
    for (int i = 0; i < m_tabWidget->count(); i++)
    {
        QWidget* wid = m_tabWidget->widget(i);
        if (DisplayWidget* pView = qobject_cast<DisplayWidget*>(wid))
        {
            pView->onFinished();
        }
    }
}

void ZTabDockWidget::onRun()
{
    for (int i = 0; i < m_tabWidget->count(); i++)
        if (DisplayWidget* pView = qobject_cast<DisplayWidget*>(m_tabWidget->widget(i)))
            pView->onRun();
}

void ZTabDockWidget::onRecord()
{
    for (int i = 0; i < m_tabWidget->count(); i++)
        if (DisplayWidget *pView = qobject_cast<DisplayWidget *>(m_tabWidget->widget(i)))
            pView->onRecord();
}

void ZTabDockWidget::onKill()
{
    for (int i = 0; i < m_tabWidget->count(); i++)
        if (DisplayWidget *pView = qobject_cast<DisplayWidget *>(m_tabWidget->widget(i)))
            pView->onKill();
}

void ZTabDockWidget::onPlayClicked(bool bChecked)
{
    for (int i = 0; i < m_tabWidget->count(); i++)
        if (DisplayWidget *pView = qobject_cast<DisplayWidget *>(m_tabWidget->widget(i)))
            pView->onPlayClicked(bChecked);
}

void ZTabDockWidget::onSliderValueChanged(int frame)
{
    for (int i = 0; i < m_tabWidget->count(); i++)
        if (DisplayWidget *pView = qobject_cast<DisplayWidget *>(m_tabWidget->widget(i)))
            pView->onSliderValueChanged(frame);
}

void ZTabDockWidget::onFinished()
{
    for (int i = 0; i < m_tabWidget->count(); i++)
        if (DisplayWidget *pView = qobject_cast<DisplayWidget *>(m_tabWidget->widget(i)))
            pView->onFinished();
}

void ZTabDockWidget::paintEvent(QPaintEvent* event)
{
    QPainter painter(this);
    painter.fillRect(rect(), QColor(36, 36, 36));
    _base::paintEvent(event);
}

bool ZTabDockWidget::event(QEvent* event)
{
    return _base::event(event);
}

void ZTabDockWidget::onDockOptionsClicked()
{
    QMenu* menu = new QMenu(this);
    QFont font("HarmonyOS Sans", 12);
    font.setBold(false);
    menu->setFont(font);

    QAction* pSplitHor = new QAction(tr("Split Left/Right"));
    QAction* pSplitVer = new QAction(tr("Split Top/Bottom"));
    QAction* pMaximize = new QAction(tr("Maximize"));
    QAction* pFloatWin = new QAction(tr("Float Window"));
    QAction* pCloseLayout = new QAction(tr("Close Layout"));

    connect(pMaximize, SIGNAL(triggered()), this, SIGNAL(maximizeTriggered()));
    connect(pFloatWin, SIGNAL(triggered()), this, SLOT(onFloatTriggered()));
    connect(pCloseLayout, &QAction::triggered, this, [=](){
        emit closeRequest();
        });
    connect(pSplitHor, &QAction::triggered, this, [=]() {
        emit splitRequest(true);
        });
    connect(pSplitVer, &QAction::triggered, this, [=]() {
        emit splitRequest(false);
        });

    menu->addAction(pSplitHor);
    menu->addAction(pSplitVer);
    menu->addSeparator();
    menu->addAction(pMaximize);
    menu->addAction(pFloatWin);
    menu->addSeparator();
    menu->addAction(pCloseLayout);
    menu->exec(QCursor::pos());
}

void ZTabDockWidget::onMaximizeTriggered()
{

}

void ZTabDockWidget::onFloatTriggered()
{
    if (isFloating()) 
	{
        setWindowFlags(m_oldFlags);
        ZenoMainWindow *pMainWin = zenoApp->getMainWindow();
        //need redock
        pMainWin->restoreDockWidget(this);
        bool bVisible = isVisible();
        if (!bVisible) {
            setVisible(true);
        }
        setFloating(false);
    } 
	else 
	{
        setFloating(true);
        m_oldFlags = windowFlags();
        if (m_debugPanel == PANEL_EDITOR) 
		{
            setParent(nullptr);
            m_newFlags = Qt::CustomizeWindowHint | Qt::Window | Qt::WindowMinimizeButtonHint |
                         Qt::WindowMaximizeButtonHint | Qt::WindowCloseButtonHint;
            setWindowFlags(m_newFlags);
            show();
        } 
		else if (m_debugPanel == PANEL_VIEW) 
		{
            //reinitialize glview is not allowed.
            setParent(nullptr);
            m_newFlags = Qt::CustomizeWindowHint | Qt::Window | Qt::WindowMinimizeButtonHint | Qt::WindowMaximizeButtonHint;
            setWindowFlags(m_newFlags);
            show();
        }
		else 
		{
			setParent(nullptr);
            m_newFlags = Qt::CustomizeWindowHint | Qt::Window | Qt::WindowCloseButtonHint;
            setWindowFlags(m_newFlags);
            show();
		}
    }
}

void ZTabDockWidget::onAddTabClicked()
{
    QMenu* menu = new QMenu(this);
    QFont font("HarmonyOS Sans", 12);
    font.setBold(false);
    menu->setFont(font);

    static QList<QString> panels = { tr("Parameter"), tr("View"), tr("Editor"), tr("Data"), tr("Logger"), tr("Light") };
    for (QString name : panels)
    {
        QAction* pAction = new QAction(name);
        connect(pAction, &QAction::triggered, this, [=]() {
            PANEL_TYPE type = title2Type(name);
            QWidget* wid = createTabWidget(type);
            if (wid)
            {
                int idx = m_tabWidget->addTab(wid, name);
                switch (panels.indexOf(name)) {
                case 0: m_debugPanel = PANEL_NODE_PARAMS; break;
                case 1: m_debugPanel = PANEL_VIEW; break;
                case 2: m_debugPanel = PANEL_EDITOR; break;
                case 3: m_debugPanel = PANEL_NODE_DATA; break;
                case 4: m_debugPanel = PANEL_LOG; break;
                case 5: m_debugPanel = PANEL_LIGHT; break;
                }
                m_tabWidget->setCurrentIndex(idx);
            }
        });
        menu->addAction(pAction);
    }
    menu->exec(QCursor::pos());
}

void ZTabDockWidget::onAddTab(PANEL_TYPE type)
{
    if (getUniqueViewport())
        return;     //because of the unsteadiness of create/delete viewport widget, we only allow to use the default one.

    QWidget *wid = createTabWidget(type);
    if (wid) {
        QString name = type2Title(type);
        int idx = m_tabWidget->addTab(wid, name);
        m_debugPanel = type;
        m_tabWidget->setCurrentIndex(idx);
    }
}

void ZTabDockWidget::onMenuActionTriggered(QAction* pAction, bool bTriggered)
{
    if (!pAction)
        return;

    int actionType = pAction->property("ActionType").toInt();
    for (int i = 0; i < m_tabWidget->count(); i++)
    {
        QWidget* wid = m_tabWidget->widget(i);
        if (DockContent_Parameter* prop = qobject_cast<DockContent_Parameter*>(wid))
        {
        }
        if (DisplayWidget* pView = qobject_cast<DisplayWidget*>(wid))
        {
            //todo: translate.
            pView->onCommandDispatched(actionType, bTriggered);
        }
        if (DockContent_Editor* pEditor = qobject_cast<DockContent_Editor*>(wid))
        {
            //undo/redo issues
            pEditor->onCommandDispatched(pAction, bTriggered);
        }
        if (ZenoSpreadsheet* pSpreadsheet = qobject_cast<ZenoSpreadsheet*>(wid))
        {

        }
        if (ZlogPanel* logPanel = qobject_cast<ZlogPanel*>(wid))
        {

        }
    }
}

void ZTabDockWidget::init(ZenoMainWindow* pMainWin)
{
    QPalette palette = this->palette();
    palette.setBrush(QPalette::Window, QColor(38, 38, 38));
    palette.setBrush(QPalette::WindowText, QColor());
    setPalette(palette);
    //...
}

bool ZTabDockWidget::isTopLevelWin()
{
    return false;
}