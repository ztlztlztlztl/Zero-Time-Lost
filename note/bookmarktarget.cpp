#include "bookmarktarget.h"
#include <QDesktopServices>
#include <QUrl>
#include <QFileInfo>
#include <QDir>
#include <QSettings>
#include <QProcess>
#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shlobj.h>
#include <shellapi.h>
#endif
QString BookmarkTarget::validate(const QJsonObject &entry){
    const auto type=entry["targetType"].toString(),target=entry["target"].toString().trimmed();
    if(type=="website"){
        if(!QStringList{"default","edge","chrome"}.contains(entry["browser"].toString("default")))return "浏览器选择无效";
        const QUrl url(target,QUrl::StrictMode);
        if(!url.isValid()||(url.scheme()!="http"&&url.scheme()!="https")||url.host().isEmpty()||!url.userInfo().isEmpty())return "请输入完整的 http / https 网址";
    }else if(type=="file"||type=="folder"){
        if(target.isEmpty()||!QDir::isAbsolutePath(target)||target.contains(QChar(0)))return "请选择文件或文件夹的绝对路径";
    }else return "请选择网站、文件或文件夹";
    return {};
}
bool BookmarkTarget::open(const QJsonObject &entry,QString *error,bool locate){
    *error=validate(entry);if(!error->isEmpty())return false;
    const auto type=entry["targetType"].toString(),target=entry["target"].toString();
    if(type=="website"){
        const auto browser=entry["browser"].toString("default");
        return openWebsite(QUrl(target),browserCandidates(browser),error);
    }
    const QFileInfo info(target);
    if(!info.exists()){*error="目标不存在或当前无法访问：\n"+QDir::toNativeSeparators(target)+"\n请检查目标是否已移动、网络盘连接和路径权限，或编辑书签路径。";return false;}
    if(type=="folder"?!info.isDir():!info.isFile()){*error="目标类型与书签不一致，请点击“编辑”重新选择文件或文件夹：\n"+QDir::toNativeSeparators(target);return false;}
#ifdef Q_OS_WIN
    if(!locate){
        const auto native=QDir::toNativeSeparators(info.absoluteFilePath());
        const auto result=reinterpret_cast<INT_PTR>(ShellExecuteW(nullptr,L"open",reinterpret_cast<LPCWSTR>(native.utf16()),nullptr,nullptr,SW_SHOWNORMAL));
        if(result>32)return true;
        *error=QString("系统无法打开目标（错误 %1），请检查默认程序、网络盘连接或路径权限").arg(result);return false;
    }
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
    if(QDesktopServices::openUrl(QUrl::fromLocalFile(!locate||type=="folder"?info.absoluteFilePath():info.absolutePath())))return true;
#endif
    *error="无法打开资源管理器";return false;
}

bool BookmarkTarget::openWebsite(const QUrl &url,const QStringList &candidates,QString *error){
    error->clear();
    const auto executable=firstExecutable(candidates);
    if(!executable.isEmpty()){
        if(QProcess::startDetached(executable,{url.toString(QUrl::FullyEncoded)}))return true;
        *error="无法启动所选浏览器，请检查安装状态。";return false;
    }
    if(QDesktopServices::openUrl(url))return true;
    *error="无法打开默认浏览器";return false;
}

QStringList BookmarkTarget::browserCandidates(const QString &browser){
    if(browser!="edge"&&browser!="chrome")return {};
    const auto executable=browser=="edge"?QString("msedge.exe"):QString("chrome.exe");
    QStringList paths;
#ifdef Q_OS_WIN
    for(const auto &root:{QString("HKEY_CURRENT_USER"),QString("HKEY_LOCAL_MACHINE")})
        for(auto format:{QSettings::Registry64Format,QSettings::Registry32Format}){
            QSettings settings(root+"\\Software\\Microsoft\\Windows\\CurrentVersion\\App Paths\\"+executable,format);
            auto path=settings.value(".").toString().trimmed();
            if(path.startsWith('"')&&path.endsWith('"'))path=path.mid(1,path.size()-2);
            if(!path.isEmpty())paths.append(path);
        }
#endif
    const auto suffix=browser=="edge"?"/Microsoft/Edge/Application/msedge.exe":"/Google/Chrome/Application/chrome.exe";
    for(const auto &variable:{"LOCALAPPDATA","ProgramFiles","ProgramFiles(x86)"}){
        const auto root=qEnvironmentVariable(variable);if(!root.isEmpty())paths.append(QDir::cleanPath(root+suffix));
    }
    paths.removeDuplicates();return paths;
}
QString BookmarkTarget::firstExecutable(const QStringList &candidates){
    for(const auto &candidate:candidates){const QFileInfo info(candidate);if(info.isFile()&&info.isExecutable())return info.absoluteFilePath();}
    return {};
}
