#include "bookmarktarget.h"
#include <QDesktopServices>
#include <QUrl>
#include <QFileInfo>
#include <QDir>
#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shlobj.h>
#endif
QString BookmarkTarget::validate(const QJsonObject &entry){
    const auto type=entry["targetType"].toString(),target=entry["target"].toString().trimmed();
    if(type=="website"){
        const QUrl url(target,QUrl::StrictMode);
        if(!url.isValid()||(url.scheme()!="http"&&url.scheme()!="https")||url.host().isEmpty()||!url.userInfo().isEmpty())return "请输入完整的 http / https 网址";
    }else if(type=="file"||type=="folder"){
        if(target.isEmpty()||!QDir::isAbsolutePath(target)||target.contains(QChar(0)))return "请选择文件或文件夹的绝对路径";
    }else return "请选择网站、文件或文件夹";
    return {};
}
bool BookmarkTarget::open(const QJsonObject &entry,QString *error){
    *error=validate(entry);if(!error->isEmpty())return false;
    const auto type=entry["targetType"].toString(),target=entry["target"].toString();
    if(type=="website"){
        if(QDesktopServices::openUrl(QUrl(target)))return true;
        *error="无法打开默认浏览器";return false;
    }
    const QFileInfo info(target);
    if(!info.exists()||(type=="folder"?!info.isDir():!info.isFile())){*error="目标不存在或类型不符，请点击书签的“编辑”修正路径";return false;}
#ifdef Q_OS_WIN
    const auto path=QDir::toNativeSeparators(info.absoluteFilePath());PIDLIST_ABSOLUTE pidl=nullptr;
    HRESULT result=SHParseDisplayName(reinterpret_cast<LPCWSTR>(path.utf16()),nullptr,&pidl,0,nullptr);
    if(SUCCEEDED(result)){
        if(type=="folder")result=SHOpenFolderAndSelectItems(pidl,0,nullptr,0);
        else{
            auto parent=ILCloneFull(pidl);ILRemoveLastID(parent);PCUITEMID_CHILD child=ILFindLastID(pidl);
            result=SHOpenFolderAndSelectItems(parent,1,&child,0);CoTaskMemFree(parent);
        }
        CoTaskMemFree(pidl);
    }
    if(SUCCEEDED(result))return true;
#else
    if(QDesktopServices::openUrl(QUrl::fromLocalFile(type=="folder"?info.absoluteFilePath():info.absolutePath())))return true;
#endif
    *error="无法打开资源管理器";return false;
}
