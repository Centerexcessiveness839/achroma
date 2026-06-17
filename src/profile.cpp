#include "utils.h"
#include <QWebEngineProfile>

namespace Achroma
{

QWebEngineProfile* mainProfile()
{
    static QWebEngineProfile* p = new QWebEngineProfile("achroma");
    return p;
}

}  // namespace Achroma
