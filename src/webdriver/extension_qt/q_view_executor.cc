#include "extension_qt/q_view_executor.h"

#include "webdriver_logging.h"
#include "webdriver_session.h"
#include "q_key_converter.h"
#include "extension_qt/widget_view_handle.h"
#include "widget_view_util.h"
#include "common_util.h"
#include "extension_qt/event_dispatcher.h"
#include "extension_qt/wd_event_dispatcher.h"

#include <QtCore/QDebug>
#if (QT_VERSION >= QT_VERSION_CHECK(5, 0, 0))
#include <QtWidgets/QApplication>
#include <QtWidgets/QDesktopWidget>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QInputDialog>
#include <QtCore/QDateTime>
#ifdef OS_ANDROID
    #include <qpa/qplatformnativeinterface.h>
    #include <jni.h> 
#endif //OS_ANDROID
#else
#include <QtGui/QApplication>
#include <QtGui/QDesktopWidget>
#include <QtGui/QMessageBox>
#include <QtGui/QInputDialog>
#endif

namespace webdriver {

QViewCmdExecutor::QViewCmdExecutor(Session* session, ViewId viewId)
    : ViewCmdExecutor(session, viewId) {

}

QViewCmdExecutor::~QViewCmdExecutor() {

};

QWidget* QViewCmdExecutor::getView(const ViewId& viewId, Error** error) {
    QWidget* pWidget = QWidgetViewUtil::getView(session_, viewId);

    if (NULL == pWidget) {
        session_->logger().Log(kWarningLogLevel, "checkView - no such view("+viewId.id()+")");
        *error = new Error(kNoSuchWindow);
        return NULL;
    }

    return pWidget;
}

void QViewCmdExecutor::GetTitle(std::string* title, Error **error) {
    QWidget* view = getView(view_id_, error);
    if (NULL == view)
        return;

    *title = view->windowTitle().toStdString();

    session_->logger().Log(kFineLogLevel, "GetTitle - "+*title);
}

void QViewCmdExecutor::GetWindowName(std::string* name, Error ** error) {
    QWidget* view = getView(view_id_, error);
    if (NULL == view)
        return;

    *name = view->windowTitle().toStdString();

    session_->logger().Log(kFineLogLevel, "GetWindowName - "+*name);
}

void QViewCmdExecutor::GetBounds(Rect *bounds, Error **error) {
    QWidget* view = getView(view_id_, error);
    if (NULL == view)
        return;

    *bounds = QCommonUtil::ConvertQRectToRect(view->geometry());
}
    
void QViewCmdExecutor::SetBounds(const Rect& bounds, Error** error) {
    QWidget* view = getView(view_id_, error);
    if (NULL == view)
        return;

    if (!view->isTopLevel()) {
        *error = new Error(kUnknownError, "Cant set bounds to non top level view.");
        return;
    }

    view->setGeometry(QCommonUtil::ConvertRectToQRect(bounds));
}

void QViewCmdExecutor::Maximize(Error** error) {
    QWidget* view = getView(view_id_, error);
    if (NULL == view)
        return;

    if (!view->isTopLevel()) {
        *error = new Error(kUnknownError, "Cant maximize non top level view.");
        return;
    }

    view->setGeometry(QApplication::desktop()->rect());    
}

void QViewCmdExecutor::GetScreenShot(std::string* png, Error** error) {
    QWidget* view = getView(view_id_, error);
    if (NULL == view)
        return;
    
    const FilePath::CharType kPngFileName[] = FILE_PATH_LITERAL("./screen.png");
    FilePath path = session_->temp_dir().Append(kPngFileName);;

    QPixmap pixmap = QPixmap::grabWidget(view);

#if defined(OS_WIN)
    session_->logger().Log(kInfoLogLevel, "Save screenshot to - " + path.MaybeAsASCII());
#elif defined(OS_POSIX)
    session_->logger().Log(kInfoLogLevel, "Save screenshot to - " + path.value());
#endif

#if defined(OS_POSIX)
    if (!pixmap.save(path.value().c_str())) 
#elif defined(OS_WIN)
    if (!pixmap.save(QString::fromUtf16((ushort*)path.value().c_str())))
#endif // OS_WIN
    {
        *error = new Error(kUnknownError, "screenshot was not captured");
        return;
    }

    if (!file_util::ReadFileToString(path, png))
        *error = new Error(kUnknownError, "Could not read screenshot file");
}

void QViewCmdExecutor::SendKeys(const string16& keys, Error** error) {
    QWidget* view = getView(view_id_, error);
    if (NULL == view)
        return;

    std::string err_msg;
    std::vector<QKeyEvent> key_events;
    int modifiers = session_->get_sticky_modifiers();

    if (!QKeyConverter::ConvertKeysToWebKeyEvents(keys,
                               session_->logger(),
                               false,
                               &modifiers,
                               &key_events,
                               &err_msg)) {
        session_->logger().Log(kSevereLogLevel, "SendKeys - cant convert keys:"+err_msg);
        *error = new Error(kUnknownError, "SendKeys - cant convert keys:"+err_msg);
        return;
    }

    session_->set_sticky_modifiers(modifiers);

    std::vector<QKeyEvent>::iterator it = key_events.begin();
    while (it != key_events.end()) {

        bool consumed = WDEventDispatcher::getInstance()->dispatch(&(*it));

        if (!consumed)
            qApp->sendEvent(view, &(*it));
        ++it;
    }
}

void QViewCmdExecutor::Close(Error** error) {
    QWidget* view = getView(view_id_, error);
    if (NULL == view)
        return;

    if (!view->isTopLevel()) {
        *error = new Error(kUnknownError, "Cant close non top level view.");
        return;
    }

    session_->logger().Log(kInfoLogLevel, "close View("+view_id_.id()+")");

    session_->RemoveView(view_id_);

    view->close();
}

void QViewCmdExecutor::SwitchTo(Error** error) {
    QWidget* view = getView(view_id_, error);
    if (NULL == view)
        return;

    session_->set_current_view(view_id_);

    session_->logger().Log(kInfoLogLevel, "SwitchTo - set current view ("+view_id_.id()+")");
}

void QViewCmdExecutor::FindElement(const ElementId& root_element, const std::string& locator, const std::string& query, ElementId* element, Error** error) {
    std::vector<ElementId> elements;
    FindElements(root_element, locator, query, &elements, error);
    if (*error == NULL && !elements.empty())
        *element = elements[0];
    else if(*error == NULL)
        *error = new Error(kNoSuchElement);
}

void QViewCmdExecutor::GetAlertMessage(std::string* text, Error** error) {
    QWidget* view = getView(view_id_, error);
    if (NULL == view)
        return;

    // QMessageBox::information(pWeb, "Alert", message->c_str(), QMessageBox::Ok);
    QMessageBox *msgBox = view->findChild<QMessageBox*>();
    if (NULL != msgBox) {
        *text = msgBox->text().toStdString();
    } else {
        QInputDialog *msgbox = view->findChild<QInputDialog*>();

        if (NULL != msgbox) {
            *text = msgbox->labelText().toStdString();
        } else {
            *error = new Error(kNoAlertOpenError);
        }
    }
}

void QViewCmdExecutor::SetAlertPromptText(const std::string& alert_prompt_text, Error** error) {
    QWidget* view = getView(view_id_, error);
    if (NULL == view)
        return;

    std::string message_text;
    GetAlertMessage(&message_text, error);
    if (*error)
        return;

    QInputDialog *alert = view->findChild<QInputDialog*>();

    if (NULL != alert) {
        alert->setTextValue(alert_prompt_text.c_str());
    } else {
        // in python ELEMENT_NOT_VISIBLE = 11
        // kNoAlertError = 27
        *error = new Error(kElementNotVisible);
    }
}

void QViewCmdExecutor::AcceptOrDismissAlert(bool accept, Error** error) {
    QWidget* view = getView(view_id_, error);
    if (NULL == view)
        return;

    QMessageBox *msgBox = view->findChild<QMessageBox*>();

    if(NULL != msgBox) {
        if(accept) {
            msgBox->accept();
        } else {
            msgBox->close();
        }
    } else {
        QInputDialog *msgbox = view->findChild<QInputDialog*>();
        if(NULL != msgbox) {
            if(accept) {
                msgbox->accept();
            } else {
                msgbox->close();
            }
        } else {
            *error = new Error(kNoAlertOpenError);
        }
    }
}

void QViewCmdExecutor::SetOrientation(const std::string &orientation, Error **error)
{
    QWidget* view = getView(view_id_, error);
    if (NULL == view)
        return;

    int screen_orientation;
    if (orientation == "LANDSCAPE")
       screen_orientation = 0;
    else if (orientation == "PORTRAIT")
       screen_orientation = 1;
    else
    {
        *error = new Error(kBadRequest, "Invalid \"orientation\" parameter");
        return;
    }

#ifdef OS_ANDROID
    QPlatformNativeInterface *interface = QApplication::platformNativeInterface();
    JavaVM *currVM = (JavaVM *)interface->nativeResourceForIntegration("JavaVM");

    JNIEnv * g_env = NULL;
    jint ret = currVM->AttachCurrentThread(&g_env, NULL);
    if (ret != JNI_OK)
    {
        *error = new Error(kUnknownError, "JNI error: AttachCurrentThread failed");
        return;
    }
    jobject activity = (jobject)interface->nativeResourceForIntegration("QtActivity");

    jclass cls = g_env->GetObjectClass(activity);
    jmethodID mid = g_env->GetMethodID(cls, "setRequestedOrientation", "(I)V");

    if (mid == 0)
    {
        *error = new Error(kUnknownError, "JNI error: Method getRequestedOrientation not found");
        return;
    }

    g_env->CallVoidMethod(activity, mid,screen_orientation);
    currVM->DetachCurrentThread();
#else
    if (((screen_orientation == 0) && (view->height() > view->width())) || 
        ((screen_orientation == 1) && (view->height() < view->width()))) 
    {
        view->setGeometry(view->x(), view->y(), view->height(), view->width());
    }

#endif
}

void QViewCmdExecutor::GetOrientation(std::string *orientation, Error **error)
{
    QWidget* view = getView(view_id_, error);
    if (NULL == view)
        return;
#ifdef OS_ANDROID
    int android_orientation;

    QPlatformNativeInterface *interface = QApplication::platformNativeInterface();
    JavaVM *currVM = (JavaVM *)interface->nativeResourceForIntegration("JavaVM");

    JNIEnv * g_env = NULL;
    jint ret = currVM->AttachCurrentThread(&g_env, NULL);
    if (ret != JNI_OK)
    {
        *error = new Error(kUnknownError, "JNI error: AttachCurrentThread failed");
        return;
    }
    jobject activity = (jobject)interface->nativeResourceForIntegration("QtActivity");

    jclass cls = g_env->GetObjectClass(activity);
    jmethodID mid = g_env->GetMethodID(cls, "getRequestedOrientation", "()I");

    if (mid == 0)
    {
        *error = new Error(kUnknownError, "JNI error: Method getRequestedOrientation not found");
        return;
    }

    android_orientation = g_env->CallIntMethod(activity, mid);
    currVM->DetachCurrentThread();

    if (android_orientation == 0)
        *orientation = "LANDSCAPE";
    else if (android_orientation == 1)
        *orientation = "PORTRAIT";
    else
    {
        *error = new Error(kUnknownError, "Unknown orientation");
        return;
    }

#else
    // This command is emulating
    if (view->height() > view->width())
        *orientation = "PORTRAIT";
    else
        *orientation = "LANDSCAPE";
#endif
}

QTouchEvent::TouchPoint QViewCmdExecutor::createTouchPoint(Qt::TouchPointState state, QPointF &point)
{
    QTouchEvent::TouchPoint touchPoint(1);
    touchPoint.setPos(point);
    touchPoint.setState(state);
    touchPoint.setPressure(1);
    return touchPoint;
}

QTouchEvent* QViewCmdExecutor::createSimpleTouchEvent(QEvent::Type eventType, Qt::TouchPointStates touchPointStates, QPointF point)
{
    QList<QTouchEvent::TouchPoint> points;
    Qt::TouchPointState touchPointState;
    if (touchPointStates & Qt::TouchPointPressed)
        touchPointState = Qt::TouchPointPressed;
    else if (touchPointStates & Qt::TouchPointReleased)
        touchPointState = Qt::TouchPointReleased;
    else {
        touchPointState = Qt::TouchPointMoved;
    }
    QTouchEvent::TouchPoint touchPoint = createTouchPoint(touchPointState, point);
    points.append(touchPoint);
    return createTouchEvent(eventType, touchPointStates, points);
}

QTouchEvent* QViewCmdExecutor::createTouchEvent(QEvent::Type eventType, Qt::TouchPointStates touchPointStates, const QList<QTouchEvent::TouchPoint> &touchPoints)
{
#if (QT_VERSION >= QT_VERSION_CHECK(5, 0, 0))
    QTouchEvent *touchEvent = new QTouchEvent(eventType, &touchDevice, Qt::NoModifier, touchPointStates, touchPoints);
    QDateTime current = QDateTime::currentDateTime();
    ulong timestame = current.toMSecsSinceEpoch() & (((qint64)1<<(sizeof(ulong)*8))-1);
    touchEvent->setTimestamp(timestame);
#else
    QTouchEvent *touchEvent = new QTouchEvent(eventType, QTouchEvent::TouchScreen, Qt::NoModifier, touchPointStates, touchPoints);
#endif
    return touchEvent;
}

} // namespace webdriver     