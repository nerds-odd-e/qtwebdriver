#include "extension_qt/web_view_executor.h"

#include "base/stringprintf.h"

#include "value_conversion_util.h"
#include "webdriver_session.h"
#include "webdriver_util.h"
#include "common_util.h"
#include "q_key_converter.h"
#include "webdriver_server.h"

#include "web_view_util.h"
#include "web_view_visualizer.h"
#include "extension_qt/widget_view_handle.h"
#include "qwebkit_proxy.h"

#include "extension_qt/event_dispatcher.h"
#include "extension_qt/wd_event_dispatcher.h"

#include <QtCore/QtGlobal>
#include <QtNetwork/QNetworkCookieJar>
#include <QtNetwork/QNetworkCookie>
#include <QtCore/QTimer>
#include <base/values.h>

#if (QT_VERSION >= QT_VERSION_CHECK(5, 0, 0))
#include <QtWidgets/QApplication>
#include <QtWebKitWidgets/QWebFrame>

#else
#include <QtGui/QApplication>
#endif
class QNetworkCookie;
namespace webdriver {

#define CHECK_VIEW_EXISTANCE    \
    if (NULL == view_) { \
        session_->logger().Log(kWarningLogLevel, "checkView - no such web view("+view_id_.id()+")"); \
        *error = new Error(kNoSuchWindow); \
        return; \
    } \
    webkitProxy_.reset(new QWebkitProxy(session_, (view_)?view_->page():NULL));

const ViewType QWebViewCmdExecutorCreator::WEB_VIEW_TYPE = 0x13f0;

QWebViewCmdExecutorCreator::QWebViewCmdExecutorCreator()
    : ViewCmdExecutorCreator() { }

QWebViewCmdExecutorCreator::~QWebViewCmdExecutorCreator() {}

ViewCmdExecutor* QWebViewCmdExecutorCreator::CreateExecutor(Session* session, ViewId viewId) const {
    QWebView* pWebView = QWebViewUtil::getWebView(session, viewId);
    if (NULL != pWebView) {
        session->logger().Log(kFineLogLevel, "Web executor for view("+viewId.id()+")");
        return new QWebViewCmdExecutor(session, viewId);
    }

    return NULL;
}

bool QWebViewCmdExecutorCreator::CanHandleView(Session* session, ViewId viewId, ViewType* viewType) const {
    QWebView* pWebView = QWebViewUtil::getWebView(session, viewId);

    if (NULL != pWebView) {
        if (NULL != viewType) *viewType = WEB_VIEW_TYPE;
        return true;
    }

    return false;
}

std::string QWebViewCmdExecutorCreator::GetViewTypeName() const {
    return "html";
}

QWebViewCmdExecutor::QWebViewCmdExecutor(Session* session, ViewId viewId)
    : QViewCmdExecutor(session, viewId) {

    view_ = QWebViewUtil::getWebView(session_, viewId);
    //webkitProxy_.reset(new QWebkitProxy(session_, (view_)?view_->page():NULL));
}

QWebViewCmdExecutor::~QWebViewCmdExecutor() {}

QWebView* QWebViewCmdExecutor::getView(const ViewId& viewId, Error** error) {
    QWebView* pWebView = QWebViewUtil::getWebView(session_, viewId);

    if (NULL == pWebView) {
        session_->logger().Log(kWarningLogLevel, "checkView - no such web view("+viewId.id()+")");
        *error = new Error(kNoSuchWindow);
        return NULL;
    }

    return pWebView;
}   

void QWebViewCmdExecutor::CanHandleUrl(const std::string& url, bool* can, Error **error) {
    QWebView* pWebView = getView(view_id_, error);

    if (NULL == pWebView) {

        *error = new Error(kNoSuchWindow);
        return;
    }
    *can = QWebViewUtil::isUrlSupported(pWebView->page(), url, error);
}

void QWebViewCmdExecutor::GetTitle(std::string* title, Error **error) {
    CHECK_VIEW_EXISTANCE

    *error = webkitProxy_->GetTitle(title);
}

void QWebViewCmdExecutor::GetWindowName(std::string* name, Error ** error) {
    CHECK_VIEW_EXISTANCE

    *error = webkitProxy_->GetWindowName(name);

    session_->logger().Log(kFineLogLevel, "GetWindowName - "+*name);
}

void QWebViewCmdExecutor::GetBounds(Rect *bounds, Error **error) {
    CHECK_VIEW_EXISTANCE

    *error = webkitProxy_->GetBounds(bounds);
}

void QWebViewCmdExecutor::GoForward(Error** error) {
    CHECK_VIEW_EXISTANCE

    *error = webkitProxy_->GoForward();
}

void QWebViewCmdExecutor::GoBack(Error** error) {
    CHECK_VIEW_EXISTANCE

    *error = webkitProxy_->GoBack();
}

void QWebViewCmdExecutor::Reload(Error** error) {
    CHECK_VIEW_EXISTANCE

    *error = webkitProxy_->Reload();

    session_->logger().Log(kFineLogLevel, "Web sync reload.");
}

void QWebViewCmdExecutor::GetSource(std::string* source, Error** error) {
    CHECK_VIEW_EXISTANCE

    *error = webkitProxy_->GetSource(source);
}

void QWebViewCmdExecutor::SendKeys(const ElementId& element, const string16& keys, Error** error) {
    CHECK_VIEW_EXISTANCE

    bool is_displayed = false;
    *error = webkitProxy_->IsElementDisplayed(element, true, &is_displayed);
    if (*error)
        return;
    
    if (!is_displayed) {
        *error = new Error(kElementNotVisible);
        return;
    }

    bool is_enabled = false;
    *error = webkitProxy_->IsElementEnabled(element, &is_enabled);
    if (*error)
        return;
    
    if (!is_enabled) {
        *error = new Error(kInvalidElementState);
        return;
    }

    *error = webkitProxy_->SetActiveElement(element);
    if (*error)
        return;

    std::string err_msg;
    std::vector<QKeyEvent> key_events;
    int modifiers = Qt::NoModifier;

    if (!QKeyConverter::ConvertKeysToWebKeyEvents(keys,
                               session_->logger(),
                               true,
                               &modifiers,
                               &key_events,
                               &err_msg)) {
        session_->logger().Log(kSevereLogLevel, "ElementSendKeys - cant convert keys:"+err_msg);
        *error = new Error(kUnknownError, "ElementSendKeys - cant convert keys:"+err_msg);
        return;
    }

    std::vector<QKeyEvent>::iterator it = key_events.begin();
    while (it != key_events.end()) {
        qApp->sendEvent(view_, &(*it));
        ++it;
    }
}

void QWebViewCmdExecutor::MouseDoubleClick(Error** error) {
    CHECK_VIEW_EXISTANCE

    QPoint point = QCommonUtil::ConvertPointToQPoint(session_->get_mouse_position());

    QMouseEvent *dbEvent = new QMouseEvent(QEvent::MouseButtonDblClick, point, Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    QMouseEvent *releaseEvent = new QMouseEvent(QEvent::MouseButtonRelease, point, Qt::LeftButton, Qt::NoButton, Qt::NoModifier);

    QApplication::postEvent(view_, dbEvent);
    QApplication::postEvent(view_, releaseEvent);
}

void QWebViewCmdExecutor::MouseButtonUp(Error** error) {
    CHECK_VIEW_EXISTANCE

    QPoint point = QCommonUtil::ConvertPointToQPoint(session_->get_mouse_position());

    QMouseEvent *releaseEvent = new QMouseEvent(QEvent::MouseButtonRelease, point, Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    QApplication::postEvent(view_, releaseEvent);
}

void QWebViewCmdExecutor::MouseButtonDown(Error** error) {
    CHECK_VIEW_EXISTANCE

    QPoint point = QCommonUtil::ConvertPointToQPoint(session_->get_mouse_position());

    QMouseEvent *pressEvent = new QMouseEvent(QEvent::MouseButtonPress, point, Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(view_, pressEvent);
}

void QWebViewCmdExecutor::MouseClick(MouseButton button, Error** error) {
    CHECK_VIEW_EXISTANCE

    QPoint point = QCommonUtil::ConvertPointToQPoint(session_->get_mouse_position());

    Qt::MouseButton mouseButton = QCommonUtil::ConvertMouseButtonToQtMouseButton(button);
    QMouseEvent *pressEvent = new QMouseEvent(QEvent::MouseButtonPress, point, mouseButton, Qt::NoButton, Qt::NoModifier);
    QMouseEvent *releaseEvent = new QMouseEvent(QEvent::MouseButtonRelease, point, mouseButton, Qt::NoButton, Qt::NoModifier);

    QApplication::postEvent(view_, pressEvent);
    QApplication::postEvent(view_, releaseEvent);
    if (Qt::RightButton == mouseButton) {
        QContextMenuEvent *contextEvent = new QContextMenuEvent(QContextMenuEvent::Mouse, point);
        QApplication::postEvent(view_, contextEvent);
    }
}

void QWebViewCmdExecutor::MouseMove(const int x_offset, const int y_offset, Error** error) {
    CHECK_VIEW_EXISTANCE

    Point prev_pos = session_->get_mouse_position();
    prev_pos.Offset(x_offset, y_offset);

    QPoint point = QCommonUtil::ConvertPointToQPoint(prev_pos);

    QMouseEvent *moveEvent = new QMouseEvent(QEvent::MouseMove, point, Qt::NoButton, Qt::NoButton, Qt::NoModifier);
    QApplication::postEvent(view_, moveEvent);

    session_->set_mouse_position(prev_pos);
}

void QWebViewCmdExecutor::MouseMove(const ElementId& element, int x_offset, const int y_offset, Error** error) {
    CHECK_VIEW_EXISTANCE

    Point location;
    *error = webkitProxy_->GetElementLocationInView(element, &location);
    if (*error)
        return;

    location.Offset(x_offset, y_offset);

    QPoint point = QCommonUtil::ConvertPointToQPoint(location);

    QMouseEvent *moveEvent = new QMouseEvent(QEvent::MouseMove, point, Qt::NoButton, Qt::NoButton, Qt::NoModifier);
    QApplication::postEvent(view_, moveEvent);

    session_->set_mouse_position(location);
}

void QWebViewCmdExecutor::MouseMove(const ElementId& element, Error** error) {
    CHECK_VIEW_EXISTANCE

    Point location;

    // element is specified, calculate the coordinate.
    *error = webkitProxy_->GetElementLocationInView(element, &location);
    if (*error)
        return;
    
    // calculate the half of the element size and translate by it.
    Size size;
    *error = webkitProxy_->GetElementSize(element, &size);
    if (*error)
        return;

    location.Offset(size.width() / 2, size.height() / 2);

    QPoint point = QCommonUtil::ConvertPointToQPoint(location);

    QMouseEvent *moveEvent = new QMouseEvent(QEvent::MouseMove, point, Qt::NoButton, Qt::NoButton, Qt::NoModifier);
    QApplication::postEvent(view_, moveEvent);

    session_->set_mouse_position(location);
}

void QWebViewCmdExecutor::ClickElement(const ElementId& element, Error** error) {
    CHECK_VIEW_EXISTANCE

    std::string tag_name;
    *error = webkitProxy_->GetElementTagName(element, &tag_name);
    if (*error)
        return;

    if (tag_name == "option") {
        bool can_be_toggled;
        *error = webkitProxy_->IsElementCanBeToggled(element, &can_be_toggled);
        if (*error)
            return;

        if (can_be_toggled) {
            *error = webkitProxy_->ToggleOptionElement(element);
        } else {
            *error = webkitProxy_->SetOptionElementSelected(element, true);
        }
    } else {
        Point location;

        *error = webkitProxy_->GetClickableLocation(element, &location);
        if (!(*error)) {
            session_->logger().Log(kFineLogLevel,
                base::StringPrintf("ClickElement at pos (%f, %f).", location.x(), location.y()));
            session_->set_mouse_position(location);
            MouseClick(kLeftButton, error);
        }
    }
}

void QWebViewCmdExecutor::GetAttribute(const ElementId& element, const std::string& key, base::Value** value, Error** error) {
    CHECK_VIEW_EXISTANCE

    *error = webkitProxy_->GetAttribute(element, key, value);
}

void QWebViewCmdExecutor::ClearElement(const ElementId& element, Error** error) {
    CHECK_VIEW_EXISTANCE

    *error = webkitProxy_->ClearElement(element);
}

void QWebViewCmdExecutor::IsElementDisplayed(const ElementId& element, bool ignore_opacity, bool* is_displayed, Error** error) {
    CHECK_VIEW_EXISTANCE

    *error = webkitProxy_->IsElementDisplayed(element, ignore_opacity, is_displayed);
}

void QWebViewCmdExecutor::IsElementEnabled(const ElementId& element, bool* is_enabled, Error** error) {
    CHECK_VIEW_EXISTANCE

    *error = webkitProxy_->IsElementEnabled(element, is_enabled);
}

void QWebViewCmdExecutor::ElementEquals(const ElementId& element1, const ElementId& element2, bool* is_equal, Error** error) {
    CHECK_VIEW_EXISTANCE

    *error = webkitProxy_->ElementEquals(element1, element2, is_equal);
}

void QWebViewCmdExecutor::GetElementLocation(const ElementId& element, Point* location, Error** error) {
    CHECK_VIEW_EXISTANCE

    *error = webkitProxy_->GetElementLocation(element, location);
}

void QWebViewCmdExecutor::GetElementLocationInView(const ElementId& element, Point* location, Error** error) {
    CHECK_VIEW_EXISTANCE

    *error = webkitProxy_->GetElementLocationInView(element, location);
}

void QWebViewCmdExecutor::GetElementTagName(const ElementId& element, std::string* tag_name, Error** error) {
    CHECK_VIEW_EXISTANCE

    *error = webkitProxy_->GetElementTagName(element, tag_name);
}

void QWebViewCmdExecutor::IsOptionElementSelected(const ElementId& element, bool* is_selected, Error** error) {
    CHECK_VIEW_EXISTANCE

    *error = webkitProxy_->IsOptionElementSelected(element, is_selected);
}

void QWebViewCmdExecutor::SetOptionElementSelected(const ElementId& element, bool selected, Error** error) {
    CHECK_VIEW_EXISTANCE

    *error = webkitProxy_->SetOptionElementSelected(element, selected);
}

void QWebViewCmdExecutor::GetElementSize(const ElementId& element, Size* size, Error** error) {
    CHECK_VIEW_EXISTANCE

    *error = webkitProxy_->GetElementSize(element, size);
}

void QWebViewCmdExecutor::ElementSubmit(const ElementId& element, Error** error) {
    CHECK_VIEW_EXISTANCE

    *error = webkitProxy_->ElementSubmit(element);
}

void QWebViewCmdExecutor::GetElementText(const ElementId& element, std::string* element_text, Error** error) {
    CHECK_VIEW_EXISTANCE

    *error = webkitProxy_->GetElementText(element, element_text);
}

void QWebViewCmdExecutor::GetElementCssProperty(const ElementId& element, const std::string& property, base::Value** value, Error** error) {
    CHECK_VIEW_EXISTANCE

    *error = webkitProxy_->GetElementCssProperty(element, property, value);
}

void QWebViewCmdExecutor::FindElement(const ElementId& root_element, const std::string& locator, const std::string& query, ElementId* element, Error** error) {
    CHECK_VIEW_EXISTANCE

    *error = webkitProxy_->FindElement(root_element, locator, query, element);
}

void QWebViewCmdExecutor::FindElements(const ElementId& root_element, const std::string& locator, const std::string& query, std::vector<ElementId>* elements, Error** error) {
    CHECK_VIEW_EXISTANCE

    *error = webkitProxy_->FindElements(root_element, locator, query, elements);
}

void QWebViewCmdExecutor::ActiveElement(ElementId* element, Error** error) {
    CHECK_VIEW_EXISTANCE

    *error = webkitProxy_->ActiveElement(element);
}

void QWebViewCmdExecutor::SwitchTo(Error** error) {
    CHECK_VIEW_EXISTANCE

    session_->set_current_view(view_id_);

    session_->logger().Log(kInfoLogLevel, "SwitchTo - set current view ("+view_id_.id()+")");

    *error = webkitProxy_->SwitchTo();
}

void QWebViewCmdExecutor::SwitchToFrameWithNameOrId(const std::string& name_or_id, Error** error) {
    CHECK_VIEW_EXISTANCE

    *error = webkitProxy_->SwitchToFrameWithNameOrId(name_or_id);
}

void QWebViewCmdExecutor::SwitchToFrameWithIndex(int index, Error** error) {
    CHECK_VIEW_EXISTANCE

    *error = webkitProxy_->SwitchToFrameWithIndex(index);
}

void QWebViewCmdExecutor::SwitchToFrameWithElement(const ElementId& element, Error** error) {
    CHECK_VIEW_EXISTANCE

    *error = webkitProxy_->SwitchToFrameWithElement(element);
}

void QWebViewCmdExecutor::SwitchToTopFrame(Error** error) {
    CHECK_VIEW_EXISTANCE

    *error = webkitProxy_->SwitchToTopFrame();
}

void QWebViewCmdExecutor::SwitchToTopFrameIfCurrentFrameInvalid(Error** error) {
    CHECK_VIEW_EXISTANCE

    *error = webkitProxy_->SwitchToTopFrameIfCurrentFrameInvalid();
}

void QWebViewCmdExecutor::NavigateToURL(const std::string& url, bool sync, Error** error) {
    CHECK_VIEW_EXISTANCE

    *error = webkitProxy_->NavigateToURL(url, sync);

    if (sync) {
        session_->logger().Log(kFineLogLevel, "Web sync load - " + url);
    } else {
        session_->logger().Log(kFineLogLevel, "Web async load - " + url);
    }
}

void QWebViewCmdExecutor::GetURL(std::string* url, Error** error) {
    CHECK_VIEW_EXISTANCE

    *error = webkitProxy_->GetURL(url);
}

void QWebViewCmdExecutor::ExecuteScript(const std::string& script, const base::ListValue* const args, base::Value** value, Error** error) {
    CHECK_VIEW_EXISTANCE

    *error = webkitProxy_->ExecuteScript(script, args, value);
}

void QWebViewCmdExecutor::ExecuteAsyncScript(const std::string& script, const base::ListValue* const args, base::Value** value, Error** error) {
    CHECK_VIEW_EXISTANCE

    *error = webkitProxy_->ExecuteAsyncScript(script, args, value);
}

void QWebViewCmdExecutor::GetAppCacheStatus(int* status, Error** error) {
    CHECK_VIEW_EXISTANCE

    *error = webkitProxy_->GetAppCacheStatus(status);
}

void QWebViewCmdExecutor::GetCookies(const std::string& url, ListValue** cookies, Error** error) {
    CHECK_VIEW_EXISTANCE

    *error = webkitProxy_->GetCookies(url, cookies);
}

void QWebViewCmdExecutor::SetCookie(const std::string& url, DictionaryValue* cookie_dict, Error** error) {
    CHECK_VIEW_EXISTANCE

    *error = webkitProxy_->SetCookie(url, cookie_dict);
}

void QWebViewCmdExecutor::DeleteCookie(const std::string& url, const std::string& cookie_name, Error** error) {
    CHECK_VIEW_EXISTANCE

    *error = webkitProxy_->DeleteCookie(url, cookie_name);
}

void QWebViewCmdExecutor::GetStorageKeys(StorageType type, base::ListValue** keys, Error** error) {
    CHECK_VIEW_EXISTANCE

    *error = webkitProxy_->GetStorageKeys(type, keys);
}

void QWebViewCmdExecutor::SetStorageItem(StorageType type, const std::string& key, const std::string& value, Error** error) {
    CHECK_VIEW_EXISTANCE

    *error = webkitProxy_->SetStorageItem(type, key, value);
}

void QWebViewCmdExecutor::ClearStorage(StorageType type, Error** error) {
    CHECK_VIEW_EXISTANCE

    *error = webkitProxy_->ClearStorage(type);
}

void QWebViewCmdExecutor::GetStorageItem(StorageType type, const std::string& key, std::string* value, Error** error) {
    CHECK_VIEW_EXISTANCE

    *error = webkitProxy_->GetStorageItem(type, key, value);
}

void QWebViewCmdExecutor::RemoveStorageItem(StorageType type, const std::string& key, std::string* value, Error** error) {
    CHECK_VIEW_EXISTANCE

    *error = webkitProxy_->RemoveStorageItem(type, key, value);
}

void QWebViewCmdExecutor::GetStorageSize(StorageType type, int* size, Error** error) {
    CHECK_VIEW_EXISTANCE

    *error = webkitProxy_->GetStorageSize(type, size);
}

void QWebViewCmdExecutor::TouchClick(const ElementId& element, Error **error) {
    CHECK_VIEW_EXISTANCE

    Point location;
    *error = webkitProxy_->GetElementLocationInView(element, &location);
    if (*error)
        return;

    QPoint point = QCommonUtil::ConvertPointToQPoint(location);

    QTouchEvent *touchBeginEvent = createSimpleTouchEvent(QEvent::TouchBegin, Qt::TouchPointPressed, QPointF(point));
    QTouchEvent *touchEndEvent = createSimpleTouchEvent(QEvent::TouchEnd, Qt::TouchPointReleased, QPointF(point));

    QApplication::postEvent(view_, touchBeginEvent);
    QApplication::postEvent(view_, touchEndEvent);

    //additional we send mouse event for QWebView
    QMouseEvent *pressEvent = new QMouseEvent(QEvent::MouseButtonPress, point, Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    QMouseEvent *releaseEvent = new QMouseEvent(QEvent::MouseButtonRelease, point, Qt::LeftButton, Qt::NoButton, Qt::NoModifier);

    QApplication::postEvent(view_, pressEvent);
    QApplication::postEvent(view_, releaseEvent);
}

void QWebViewCmdExecutor::TouchDoubleClick(const ElementId& element, Error **error) {
    CHECK_VIEW_EXISTANCE

    Point location;
    *error = webkitProxy_->GetElementLocationInView(element, &location);
    if (*error)
        return;

    QPoint point = QCommonUtil::ConvertPointToQPoint(location);

    view_->setZoomFactor(2/view_->zoomFactor());
    view_->page()->mainFrame()->scroll(point.x(), point.y());
}

void QWebViewCmdExecutor::TouchDown(const int &x, const int &y, Error **error) {
    CHECK_VIEW_EXISTANCE

    QPoint point = QCommonUtil::ConvertPointToQPoint(Point(x, y));

    QTouchEvent *touchBeginEvent = createSimpleTouchEvent(QEvent::TouchBegin, Qt::TouchPointPressed, point);

    QApplication::postEvent(view_, touchBeginEvent);
}

void QWebViewCmdExecutor::TouchUp(const int &x, const int &y, Error **error) {
    CHECK_VIEW_EXISTANCE

    QPoint point = QCommonUtil::ConvertPointToQPoint(Point(x, y));

    QTouchEvent *touchEndEvent = createSimpleTouchEvent(QEvent::TouchEnd, Qt::TouchPointReleased, point);

    QApplication::postEvent(view_, touchEndEvent);
}

void QWebViewCmdExecutor::TouchMove(const int &x, const int &y, Error **error) {
    CHECK_VIEW_EXISTANCE
        
    QPoint point = QCommonUtil::ConvertPointToQPoint(Point(x, y));

    QTouchEvent *touchMoveEvent = createSimpleTouchEvent(QEvent::TouchUpdate, Qt::TouchPointMoved, point);

    QApplication::postEvent(view_, touchMoveEvent);
}

void QWebViewCmdExecutor::TouchLongClick(const ElementId& element, Error **error) {
    CHECK_VIEW_EXISTANCE

    Point location;
    *error = webkitProxy_->GetElementLocationInView(element, &location);
    if (*error)
        return;

    QPoint point = QCommonUtil::ConvertPointToQPoint(location);

    QContextMenuEvent *contextEvent = new QContextMenuEvent(QContextMenuEvent::Other, point, view_->mapToGlobal(point));
    qApp->postEvent(view_, contextEvent);
}

void QWebViewCmdExecutor::TouchScroll(const int &xoffset, const int &yoffset, Error **error) {
    CHECK_VIEW_EXISTANCE

    view_->page()->mainFrame()->scroll(xoffset, yoffset);
}

void QWebViewCmdExecutor::TouchScroll(const ElementId &element, const int &xoffset, const int &yoffset, Error **error) {
    CHECK_VIEW_EXISTANCE

    view_->page()->mainFrame()->scroll(-xoffset, -yoffset);
}

void QWebViewCmdExecutor::TouchFlick(const int &xSpeed, const int &ySpeed, Error **error) {
    CHECK_VIEW_EXISTANCE

    view_->page()->mainFrame()->scroll(xSpeed*3, ySpeed*3);
}

void QWebViewCmdExecutor::TouchFlick(const ElementId &element, const int &xoffset, const int &yoffset, const int &speed, Error **error) {
    CHECK_VIEW_EXISTANCE

    view_->page()->mainFrame()->scroll(-xoffset*(speed+1), -yoffset*(speed+1));
}

void QWebViewCmdExecutor::GetBrowserLog(base::ListValue** browserLog, Error **error) {
    CHECK_VIEW_EXISTANCE

    *error = webkitProxy_->GetBrowserLog(browserLog);
}

void QWebViewCmdExecutor::GetPlayerState(const ElementId& element, PlayerState *state, Error **error) {
    CHECK_VIEW_EXISTANCE

    *error = webkitProxy_->GetPlayerState(element, state);
}

void QWebViewCmdExecutor::SetPlayerState(const ElementId& element, PlayerState state, Error **error) {
    CHECK_VIEW_EXISTANCE

    *error = webkitProxy_->SetPlayerState(element, state);
}

void QWebViewCmdExecutor::GetPlayerVolume(const ElementId& element, double *volume, Error **error) {
    CHECK_VIEW_EXISTANCE

    *error = webkitProxy_->GetPlayerVolume(element, volume);
}

void QWebViewCmdExecutor::SetPlayerVolume(const ElementId& element, double volume, Error **error)
{
    CHECK_VIEW_EXISTANCE

    *error = webkitProxy_->SetPlayerVolume(element, volume);
}

void QWebViewCmdExecutor::GetPlayingPosition(const ElementId& element, double *position, Error **error)
{
    CHECK_VIEW_EXISTANCE

    *error = webkitProxy_->GetPlayingPosition(element, position);
}

void QWebViewCmdExecutor::SetPlayingPosition(const ElementId& element, double position, Error **error)
{
    CHECK_VIEW_EXISTANCE

            *error = webkitProxy_->SetPlayingPosition(element, position);
}

void QWebViewCmdExecutor::SetPlaybackSpeed(const ElementId &element, double speed, Error **error)
{
    CHECK_VIEW_EXISTANCE

    *error = webkitProxy_->SetPlaybackSpeed(element, speed);
}

void QWebViewCmdExecutor::GetPlaybackSpeed(const ElementId &element, double *speed, Error **error)
{
    CHECK_VIEW_EXISTANCE

            *error = webkitProxy_->GetPlaybackSpeed(element, speed);
}

void QWebViewCmdExecutor::SetMute(const ElementId &element, bool mute, Error **error)
{
    CHECK_VIEW_EXISTANCE

    *error = webkitProxy_->SetMute(element, mute);
}

void QWebViewCmdExecutor::GetMute(const ElementId &element, bool *mute, Error **error)
{
    CHECK_VIEW_EXISTANCE

    *error = webkitProxy_->GetMute(element, mute);
}

void QWebViewCmdExecutor::VisualizerSource(std::string* source, Error** error) {
    CHECK_VIEW_EXISTANCE

    QWebViewVisualizerSourceCommand command(this, session_, view_);
    command.Execute(source, error);
}

void QWebViewCmdExecutor::VisualizerShowPoint(Error** error) {
    CHECK_VIEW_EXISTANCE

    QWebViewVisualizerShowPointCommand command(this, session_, view_);
    command.Execute(error);
}

void QWebViewCmdExecutor::SetOnline(bool online, Error** error) {
    CHECK_VIEW_EXISTANCE

    *error = webkitProxy_->SetOnline(online);
}

void QWebViewCmdExecutor::IsOnline(bool* online, Error** error) {
    CHECK_VIEW_EXISTANCE

    *error = webkitProxy_->IsOnline(online);
}


} //namespace webdriver 