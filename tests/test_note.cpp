#include <QtTest>
#include "testdialogs.h"
#include <QtWidgets>
#include "notewidget.h"
#include "bookmarktarget.h"
#include "configmanager.h"
#include "noterepository.h"
#include "cryptotool.h"
class UrlReceiver:public QObject {
    Q_OBJECT
public:
    QList<QUrl> urls;
public slots:
    void opened(const QUrl &url){urls.append(url);}
};
class NoteTests:public QObject {
    Q_OBJECT
    QTemporaryDir settings;
    NoteCard *card(NoteWidget &page,const QString &id){for(auto c:page.findChildren<NoteCard*>())if(c->id()==id)return c;return nullptr;}
    void seed(NoteDepartment &dept,int count,int kind=2){
        for(int i=0;i<count;++i)QVERIFY(dept.save({{"id",QString::number(i)},{"kind",kind},{"title",QString("标题 %1").arg(i)},{"body","正文内容"},{"targetType","website"},{"target","https://example.edu/"},{"tags",i%2?"学习":"生活"}}));
    }
    void snapshot(NoteWidget &page,const QString &name){
        const auto output=qEnvironmentVariable("ZTL_QA_DIR");if(!output.isEmpty()){QCoreApplication::processEvents();QTest::qWait(30);QDir().mkpath(output);page.grab().save(output+"/"+name+".png");}
    }
    void snapshotDialog(QDialog *dialog,const QString &name){
        const auto output=qEnvironmentVariable("ZTL_QA_DIR");if(!output.isEmpty()){QCoreApplication::processEvents();dialog->grab().save(output+"/"+name+".png");}
    }
private slots:
    void initTestCase(){
        QCoreApplication::setOrganizationName("ZTL-Tests");QCoreApplication::setApplicationName("Note");
        qputenv("ZTL_DATA_DIR",settings.path().toUtf8());QSettings::setDefaultFormat(QSettings::IniFormat);QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,settings.path());
        ConfigManager::getInstance()->initialize();ConfigManager::getInstance()->setNoteTags({"学习","生活","项目","收藏"});qApp->setFont(QFont("Microsoft YaHei UI",10));
    }
    void migrationAndPermanentDeletion(){
        QTemporaryDir dir;const auto path=dir.filePath("vault");
        NoteRepository index(path+".notes.json");
        QVERIFY(index.save({QJsonObject{{"id","paper"},{"kind",2},{"title","旧纸条"},{"tags","学习,生活,项目,收藏"}},
                            QJsonObject{{"id","bookmark"},{"kind",1},{"title","旧书签"},{"body","https://example.edu/"}},
                            QJsonObject{{"id","trash"},{"kind",2},{"title","旧垃圾"},{"deleted",true}}},false));
        VaultRepository vault(path);NoteDepartment dept(&vault);QCOMPARE(dept.catalog().size(),2);QVERIFY(!dept.migrationNotice().isEmpty());
        QCOMPARE(dept.entry("paper")["tags"].toString(),QString("学习,生活,项目"));QCOMPARE(dept.entry("bookmark")["targetType"].toString(),QString("website"));
        QCOMPARE(dept.entry("bookmark")["target"].toString(),QString("https://example.edu/"));QVERIFY(!dept.entry("bookmark").contains("body"));
        auto e=dept.entry("paper");const auto created=e["createdAt"];e["tags"]="学习,生活,项目,收藏";QVERIFY(!dept.save(e));e["tags"]="学习";e["title"]="改名";QVERIFY(dept.save(e));QCOMPARE(dept.entry("paper")["createdAt"],created);
        QVERIFY(dept.remove("paper"));NoteDepartment reopened(&vault);QVERIFY(reopened.entry("paper").isEmpty());QVERIFY(reopened.entry("trash").isEmpty());
    }
    void bookmarkValidation(){
        for(const auto &url:{"file:///C:/windows","javascript:alert(1)","mailto:test@example.com","https://","example.edu","https://user:pass@example.com"})QVERIFY(!BookmarkTarget::validate({{"targetType","website"},{"target",url}}).isEmpty());
        QVERIFY(BookmarkTarget::validate({{"targetType","website"},{"target","https://example.edu/path?q=1"}}).isEmpty());
        QVERIFY(!BookmarkTarget::validate({{"targetType","file"},{"target","relative.exe"}}).isEmpty());
        QTemporaryDir dir;QString error;QVERIFY(!BookmarkTarget::open({{"targetType","file"},{"target",dir.filePath("missing.exe")}},&error));QVERIFY(!error.isEmpty());
        QFile f(dir.filePath("a.txt"));QVERIFY(f.open(QIODevice::WriteOnly));f.close();QVERIFY(!BookmarkTarget::open({{"targetType","folder"},{"target",f.fileName()}},&error));
    }
    void packingAndIndependentEditors(){
        QTemporaryDir dir;VaultRepository vault(dir.filePath("vault"));NoteDepartment dept(&vault);seed(dept,20);
        NoteWidget page(&dept);page.resize(1200,780);page.show();QTest::qWait(30);
        auto first=card(page,"19"),moved=card(page,"15");QVERIFY(first&&moved);QCOMPARE(first->width(),first->height());QVERIFY(first->width()>=200);const int cell=first->width();
        QTest::mouseClick(first,Qt::LeftButton);QVERIFY(first->property("selected").toBool());QVERIFY(!first->expanded());
        QTest::mouseDClick(moved,Qt::LeftButton);QVERIFY(moved->expanded());QCOMPARE(moved->size(),QSize(cell*3+32,cell*3+32));QCOMPARE(moved->x(),0);QVERIFY(moved->y()>first->y());
        // Five columns: four precede the moved card. The remaining cell stays empty.
        QCOMPARE(card(page,"16")->y(),0);QCOMPARE(card(page,"14")->y(),moved->y());QVERIFY(card(page,"14")->x()>moved->geometry().right());
        QTest::mouseDClick(first,Qt::LeftButton);QVERIFY(first->expanded());
        for(auto a:page.findChildren<NoteCard*>())for(auto b:page.findChildren<NoteCard*>())if(a!=b&&!a->isHidden()&&!b->isHidden())QVERIFY(!a->geometry().intersects(b->geometry()));
        auto fields=first->findChild<NoteFields*>();auto movedFields=moved->findChild<NoteFields*>();QVERIFY(fields&&movedFields);
        fields->body->setFocus();fields->body->setPlainText("未离开焦点的草稿");auto cursor=fields->body->textCursor();cursor.setPosition(3);fields->body->setTextCursor(cursor);
        movedFields->title->setText("另外一张卡片");QVERIFY(moved->save());QCoreApplication::processEvents();
        QCOMPARE(first->findChild<NoteFields*>(),fields);QCOMPARE(fields->body->toPlainText(),QString("未离开焦点的草稿"));QCOMPARE(fields->body->textCursor().position(),3);QCOMPARE(dept.entry("19")["body"].toString(),QString("正文内容"));
        snapshot(page,"note-expanded");page.resize(420,780);QTest::qWait(30);QVERIFY(first->width()<=page.findChild<QScrollArea*>("noteScroll")->viewport()->width());snapshot(page,"note-narrow");
        QVERIFY(page.prepareLeave());QVERIFY(!first->expanded());QVERIFY(!moved->expanded());QCOMPARE(dept.entry("19")["body"].toString(),QString("未离开焦点的草稿"));
    }
    void autosaveFiltersAndFailure(){
        QTemporaryDir dir;const auto path=dir.filePath("vault");VaultRepository vault(path);NoteDepartment dept(&vault);seed(dept,3);
        NoteWidget page(&dept);page.resize(1000,760);page.show();QTest::qWait(20);auto c=card(page,"2");QTest::mouseDClick(c,Qt::LeftButton);auto fields=c->findChild<NoteFields*>();
        fields->body->setFocus();QCoreApplication::processEvents();fields->body->setPlainText("失焦后的新正文");page.findChild<QLineEdit*>("noteSearch")->setFocus();QTRY_COMPARE(dept.entry("2")["body"].toString(),QString("失焦后的新正文"));
        fields->title->setText("");QVERIFY(!page.prepareLeave());QVERIFY(c->expanded());
        auto tabs=page.findChild<QTabBar*>("noteTabs");tabs->setCurrentIndex(1);QCOMPARE(tabs->currentIndex(),0);QCOMPARE(fields->title->text(),QString());
        auto search=page.findChild<QLineEdit*>("noteSearch");search->setText("nothing");QCOMPARE(search->text(),QString());QVERIFY(!c->isHidden());
        fields->title->setText("有效标题");QVERIFY(c->save());
        // A failed atomic write retains both the editor draft and stored snapshot.
        QVERIFY(QFile::rename(path+".notes.json",path+".saved"));QVERIFY(QDir().mkdir(path+".notes.json"));fields->body->setPlainText("磁盘故障时的草稿");QVERIFY(!page.prepareLeave());QVERIFY(c->expanded());QCOMPARE(fields->body->toPlainText(),QString("磁盘故障时的草稿"));QCOMPARE(dept.entry("2")["body"].toString(),QString("失焦后的新正文"));
        QVERIFY(QDir().rmdir(path+".notes.json"));QVERIFY(QFile::rename(path+".saved",path+".notes.json"));QVERIFY(c->save());
        search->setText("磁盘故障");QVERIFY(!c->isHidden());QVERIFY(card(page,"1")->isHidden());
        auto filter=page.findChild<QComboBox*>("noteTagFilter");filter->setCurrentIndex(filter->findData("学习"));QVERIFY(c->isHidden());QVERIFY(!c->expanded());
        search->clear();filter->setCurrentIndex(0);QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);QTest::mouseDClick(c,Qt::LeftButton);fields=c->findChild<NoteFields*>();fields->title->setText("");
        QVERIFY(!c->findChild<QPushButton*>("noteDiscard"));QVERIFY(!page.prepareLeave());fields->title->setText("修正标题");QVERIFY(page.prepareLeave());QVERIFY(!c->expanded());
    }
    void bookmarkSingleAndDoubleClick(){
        QTemporaryDir dir;VaultRepository vault(dir.filePath("vault"));NoteDepartment dept(&vault);seed(dept,1,1);NoteWidget page(&dept);page.resize(900,740);page.show();page.findChild<QTabBar*>("noteTabs")->setCurrentIndex(1);QTest::qWait(20);auto c=card(page,"0");
        UrlReceiver receiver;QDesktopServices::setUrlHandler("https",&receiver,"opened");
        QTest::mouseClick(c,Qt::LeftButton);QTest::mouseDClick(c,Qt::LeftButton);QCOMPARE(receiver.urls.size(),1);QTest::qWait(QApplication::doubleClickInterval()+30);QVERIFY(!c->expanded());
        QTest::mouseClick(c,Qt::LeftButton);QTest::qWait(QApplication::doubleClickInterval()+80);QVERIFY(!c->expanded());QVERIFY(!c->property("selected").toBool());
        QTest::mouseClick(c,Qt::LeftButton);QVERIFY(c->property("selected").toBool());
        QTest::mouseClick(page.findChild<QLineEdit*>("noteSearch"),Qt::LeftButton);QVERIFY(!c->property("selected").toBool());
        c->findChild<QPushButton*>("noteBookmarkEdit")->click();QVERIFY(c->expanded());QTest::mouseDClick(c,Qt::LeftButton);QCOMPARE(receiver.urls.size(),1);QVERIFY(!c->findChild<QPushButton*>("noteDiscard"));snapshot(page,"note-bookmark");QDesktopServices::unsetUrlHandler("https");
    }
    void createAndDismiss(){
        QTemporaryDir dir;VaultRepository vault(dir.filePath("vault"));NoteDepartment dept(&vault);NoteWidget page(&dept);page.resize(1000,760);page.show();QTest::qWait(20);
        auto add=page.findChild<QPushButton*>("noteAdd");QTimer::singleShot(10000,&page,[]{qFatal("Create test timed out");});
        QTimer cancel;cancel.setInterval(20);connect(&cancel,&QTimer::timeout,this,[]{for(auto w:QApplication::topLevelWidgets())if(w->objectName()=="noteEditor"&&w->isVisible()){QTest::mouseClick(w->windowHandle(),Qt::LeftButton,Qt::NoModifier,QPoint(-8,-8));QVERIFY(w->isVisible());QTest::keyClick(w,Qt::Key_Escape);}});cancel.start();add->click();cancel.stop();QCOMPARE(dept.catalog().size(),0);
        bool invalidRetained=false;QTimer fill;fill.setInterval(20);connect(&fill,&QTimer::timeout,this,[&]{
            for(auto w:QApplication::topLevelWidgets())if(w->objectName()=="noteEditor"&&w->isVisible()){
                auto editor=qobject_cast<QDialog*>(w);auto fields=editor->findChild<NoteFields*>();auto create=editor->findChild<QPushButton*>("noteCreate");
                create->click();invalidRetained=editor->isVisible()&&fields->findChild<QLabel*>("noteError")->isVisible();
                QTimer::singleShot(10,[]{if(auto confirm=qobject_cast<QMessageBox*>(activeTestDialog()))confirm->button(QMessageBox::No)->click();});QTest::keyClick(editor,Qt::Key_Escape);invalidRetained=invalidRetained&&editor->isVisible();
                fields->title->setText("新增纸条");fields->body->setPlainText("创建时填写的内容");create->click();return;
            }
        });fill.start();add->click();fill.stop();QVERIFY(invalidRetained);QCOMPARE(dept.catalog().size(),1);auto c=card(page,dept.catalog().first().toObject()["id"].toString());QVERIFY(c);QVERIFY(!c->expanded());QCOMPARE(dept.entry(c->id())["body"].toString(),QString("创建时填写的内容"));snapshot(page,"note-created");QTest::mouseDClick(c,Qt::LeftButton);
        QTimer::singleShot(10,[]{if(auto confirm=qobject_cast<QMessageBox*>(activeTestDialog()))confirm->button(QMessageBox::Yes)->click();});c->findChild<QPushButton*>("noteDelete")->click();QVERIFY(dept.catalog().isEmpty());
    }
    void secretLockAndPermanentDeletion(){
        QTemporaryDir dir;const auto path=dir.filePath("vault");VaultRepository vault(path);NoteDepartment dept(&vault);QVERIFY(dept.create("test-master-password"));QVERIFY(dept.save({{"id","secret"},{"kind",0},{"title","秘密标题"},{"body","保密正文"}}));dept.lock();
        QVERIFY(!dept.remove("secret"));NoteWidget page(&dept);page.resize(950,740);page.show();page.findChild<QTabBar*>("noteTabs")->setCurrentIndex(2);QTest::qWait(20);auto c=card(page,"secret");QVERIFY(c);QTest::mouseClick(c,Qt::LeftButton);QVERIFY(!dept.unlocked());
        int authentications=0,stage=0;QTimer responder;responder.setInterval(20);
        connect(&responder,&QTimer::timeout,this,[&]{
            if(auto password=qobject_cast<QInputDialog*>(activeTestDialog())){++authentications;snapshotDialog(password,"note-design-password");password->setTextValue("test-master-password");QTest::keyClick(password->findChild<QLineEdit*>(),Qt::Key_Return);return;}
            auto editor=qobject_cast<QDialog*>(activeTestDialog());if(!editor||editor->objectName()!="noteSecretEditor")return;
            auto fields=editor->findChild<NoteFields*>();
            if(stage==0){QVERIFY(!fields->tags);fields->body->setPlainText("锁屏前的修改");snapshotDialog(editor,"note-design-secret-editor");stage=1;dept.lock();}
            else if(stage==1){stage=2;QTimer::singleShot(10,[]{if(auto confirm=qobject_cast<QMessageBox*>(activeTestDialog()))confirm->button(QMessageBox::Yes)->click();});for(auto b:editor->findChildren<QPushButton*>())if(b->text()=="删除"){b->click();break;}}
        });
        QTimer::singleShot(20000,&page,[]{qFatal("Secret test exceeded five seconds");});responder.start();QTest::mouseDClick(c,Qt::LeftButton);QCOMPARE(authentications,1);QVERIFY(!dept.unlocked());QVERIFY(dept.unlock("test-master-password"));QCOMPARE(dept.entry("secret")["body"].toString(),QString("锁屏前的修改"));dept.lock();
        QTest::mouseDClick(c,Qt::LeftButton);responder.stop();QCOMPARE(authentications,2);QVERIFY(!dept.unlocked());QVERIFY(dept.entry("secret").isEmpty());QVERIFY(dept.unlock("test-master-password"));QVERIFY(dept.entries().isEmpty());dept.lock();
    }
    void secretInvalidDraftLocksAndRecovers(){
        QTemporaryDir dir;VaultRepository vault(dir.filePath("vault"));NoteDepartment dept(&vault);
        QVERIFY(dept.create("test-master-password"));QVERIFY(dept.save({{"id","recovery"},{"kind",0},{"title","正式标题"},{"body","原正文"}}));dept.lock();
        NoteWidget page(&dept);page.resize(950,740);page.show();page.findChild<QTabBar*>("noteTabs")->setCurrentIndex(2);QTest::qWait(20);
        QSignalSpy failures(&page,&NoteWidget::saveFailed);int stage=0;bool recovered=false;
        QTimer responder;responder.setInterval(20);connect(&responder,&QTimer::timeout,this,[&]{
            if(auto password=qobject_cast<QInputDialog*>(activeTestDialog())){password->setTextValue("test-master-password");password->accept();return;}
            auto dialog=qobject_cast<QDialog*>(activeTestDialog());if(!dialog||dialog->objectName()!="noteSecretEditor")return;
            auto fields=dialog->findChild<NoteFields*>();
            if(stage==0){stage=1;fields->title->clear();fields->body->setPlainText("锁定时尚未保存的正文");dept.lock();}
            else if(stage==1){
                stage=2;recovered=fields->title->text().isEmpty()&&fields->body->toPlainText()=="锁定时尚未保存的正文"&&fields->findChild<QLabel*>("noteSaveStatus")->text().contains("已恢复");
                fields->title->setText("恢复后的标题");dialog->reject();
            }
        });
        responder.start();QTest::mouseDClick(card(page,"recovery"),Qt::LeftButton);QVERIFY(!dept.unlocked());QCOMPARE(failures.size(),1);QVERIFY(!vault.read()["drafts"].toArray().isEmpty());
        QTest::mouseDClick(card(page,"recovery"),Qt::LeftButton);responder.stop();QVERIFY(recovered);QVERIFY(!dept.unlocked());QVERIFY(vault.read()["drafts"].toArray().isEmpty());
        QVERIFY(dept.unlock("test-master-password"));QCOMPARE(dept.entry("recovery")["title"].toString(),QString("恢复后的标题"));QCOMPARE(dept.entry("recovery")["body"].toString(),QString("锁定时尚未保存的正文"));dept.lock();
    }
    void secretCloseGuard(){
        QTemporaryDir dir;VaultRepository vault(dir.filePath("vault"));NoteDepartment dept(&vault);QVERIFY(dept.create("test-master-password"));QVERIFY(dept.save({{"id","guard"},{"kind",0},{"title","保存的标题"},{"body","保存的内容"}}));dept.lock();
        NoteWidget page(&dept);page.resize(900,740);page.show();page.findChild<QTabBar*>("noteTabs")->setCurrentIndex(2);QTest::qWait(20);
        bool stayed=false;QTimer responder;responder.setInterval(20);connect(&responder,&QTimer::timeout,this,[&]{
            if(auto password=qobject_cast<QInputDialog*>(activeTestDialog())){password->setTextValue("test-master-password");password->accept();return;}
            auto editor=qobject_cast<QDialog*>(activeTestDialog());if(!editor||editor->objectName()!="noteSecretEditor")return;
            auto fields=editor->findChild<NoteFields*>();QVERIFY(!editor->findChild<QPushButton*>("noteSecretSave"));fields->title->setText("");fields->body->setPlainText("自动保存的内容");editor->reject();stayed=editor->isVisible()&&fields->body->toPlainText()=="自动保存的内容";
            fields->title->setText("修正标题");editor->reject();
        });QTimer::singleShot(20000,&page,[]{qFatal("Secret close test timed out");});responder.start();QTest::mouseDClick(card(page,"guard"),Qt::LeftButton);responder.stop();QVERIFY(stayed);QVERIFY(!dept.unlocked());QVERIFY(dept.unlock("test-master-password"));QCOMPARE(dept.entry("guard")["body"].toString(),QString("自动保存的内容"));dept.lock();
    }
    void selectionAndDebouncedSave(){
        QTemporaryDir dir;VaultRepository vault(dir.filePath("vault"));NoteDepartment dept(&vault);seed(dept,2);NoteWidget page(&dept);page.resize(1000,760);page.show();QTest::qWait(30);
        auto first=card(page,"1"),second=card(page,"0");QTest::mouseClick(first,Qt::LeftButton);QVERIFY(first->property("selected").toBool());QTest::mouseClick(second,Qt::LeftButton);QVERIFY(!first->property("selected").toBool());QVERIFY(second->property("selected").toBool());
        QTest::mouseClick(second,Qt::LeftButton);QVERIFY(!second->property("selected").toBool());QTest::mouseClick(second,Qt::LeftButton);QVERIFY(second->property("selected").toBool());
        auto canvas=page.findChild<QWidget*>("noteCanvas");QTest::mouseClick(canvas,Qt::LeftButton,Qt::NoModifier,QPoint(canvas->width()-10,canvas->height()-10));QVERIFY(!second->property("selected").toBool());
        QTest::mouseDClick(first,Qt::LeftButton);auto fields=first->findChild<NoteFields*>();fields->body->setFocus();QCoreApplication::processEvents();fields->body->setPlainText("输入后自动保存");QTRY_COMPARE_WITH_TIMEOUT(dept.entry("1")["body"].toString(),QString("输入后自动保存"),1500);QCOMPARE(first->findChild<NoteFields*>(),fields);QVERIFY(!first->findChild<QPushButton*>("noteDiscard"));
    }
    void initializeInSettings(){
        QTemporaryDir dir;VaultRepository vault(dir.filePath("vault"));NoteDepartment dept(&vault);NoteWidget page(&dept);page.resize(900,740);page.show();page.findChild<QTabBar*>("noteTabs")->setCurrentIndex(2);int passwords=0;
        QTimer responder;responder.setInterval(20);connect(&responder,&QTimer::timeout,this,[&]{if(auto password=qobject_cast<QInputDialog*>(activeTestDialog())){++passwords;password->setTextValue("test-master-password");password->accept();}else if(auto message=qobject_cast<QMessageBox*>(activeTestDialog()))message->accept();});
        QTimer::singleShot(15000,&page,[]{qFatal("Initialization timed out");});responder.start();page.findChild<QPushButton*>("noteAdd")->click();QCOMPARE(passwords,0);QVERIFY(!dept.exists());page.manage("初始化／升级秘密加密");responder.stop();QCOMPARE(passwords,2);QVERIFY(dept.creationReady());QVERIFY(!dept.unlocked());
    }
    void createSecretWithoutAuthentication(){
        QTemporaryDir dir;VaultRepository vault(dir.filePath("vault"));NoteDepartment dept(&vault);QVERIFY(dept.create("test-master-password"));dept.lock();
        NoteWidget page(&dept);page.resize(900,740);page.show();page.findChild<QTabBar*>("noteTabs")->setCurrentIndex(2);int passwords=0;QTimer fill;fill.setInterval(20);connect(&fill,&QTimer::timeout,this,[&]{
            if(auto password=qobject_cast<QInputDialog*>(activeTestDialog())){++passwords;password->reject();}
            if(auto editor=qobject_cast<QDialog*>(activeTestDialog()))if(editor->objectName()=="noteEditor"){auto fields=editor->findChild<NoteFields*>();fields->title->setText("免密新增");fields->body->setPlainText("首次查看前也必须加密的正文");editor->findChild<QPushButton*>("noteCreate")->click();}
        });QTimer::singleShot(10000,&page,[]{qFatal("Secret creation timed out");});fill.start();page.findChild<QPushButton*>("noteAdd")->click();fill.stop();QCOMPARE(passwords,0);QVERIFY(!dept.unlocked());QCOMPARE(dept.catalog().size(),1);auto c=card(page,dept.catalog().first().toObject()["id"].toString());QVERIFY(c);QVERIFY(!c->expanded());QVERIFY(!dept.entry(c->id()).contains("body"));QVERIFY(dept.unlock("test-master-password"));QCOMPARE(dept.entry(c->id())["body"].toString(),QString("首次查看前也必须加密的正文"));dept.lock();
    }
    void createFileAndFolderBookmarks(){
        const bool native=qEnvironmentVariableIsSet("ZTL_NATIVE_PICKER_QA");
        struct NativeDialogSetting {bool previous=QCoreApplication::testAttribute(Qt::AA_DontUseNativeDialogs);~NativeDialogSetting(){QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs,previous);}} nativeSetting;
        QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs,!native);
        QTemporaryDir dir;const auto target=dir.filePath("有空格的 file.txt");QFile file(target);QVERIFY(file.open(QIODevice::WriteOnly));file.close();VaultRepository vault(dir.filePath("vault"));NoteDepartment dept(&vault);NoteWidget page(&dept);page.resize(1000,760);page.show();page.findChild<QTabBar*>("noteTabs")->setCurrentIndex(1);
        
        for(const auto &type:{QString("file"),QString("folder")}){
            bool retained=false;QTimer fill;fill.setInterval(30);connect(&fill,&QTimer::timeout,this,[&]{
                auto editor=qobject_cast<QDialog*>(activeTestDialog());if(!editor||editor->objectName()!="noteEditor")return;
                auto fields=editor->findChild<NoteFields*>();fields->title->setText(type);fields->targetType->setCurrentIndex(fields->targetType->findData(type));
                if(native){
                    fields->target->setText(QDir::toNativeSeparators(type=="file"?target:dir.path()));
                    const auto before=fields->target->text();
                    qInfo()<<"Native cancel check"<<type;fields->findChild<QPushButton*>("noteBrowse")->click();
                    QVERIFY(editor->isVisible());QCOMPARE(fields->title->text(),type);QCOMPARE(fields->target->text(),before);
                    qInfo()<<"Native select check"<<type;
                }
                QTimer picker;picker.setInterval(40);connect(&picker,&QTimer::timeout,this,[&]{
                    for(auto chooser:editor->findChildren<QFileDialog*>()){
                        // Exercise the focus change that used to close Qt::Popup.
                        QEvent deactivated(QEvent::WindowDeactivate);QCoreApplication::sendEvent(editor,&deactivated);
                        const auto selectedPath=type=="file"?target:dir.path();chooser->setDirectory(QFileInfo(selectedPath).absolutePath());
                        if(auto input=chooser->findChild<QLineEdit*>("fileNameEdit"))input->setText(QDir::toNativeSeparators(selectedPath));else chooser->selectFile(selectedPath);QMetaObject::invokeMethod(chooser,"accept",Qt::DirectConnection);return;
                    }
                });if(!native)picker.start();fields->findChild<QPushButton*>("noteBrowse")->click();picker.stop();retained=editor->isVisible()&&!fields->target->text().isEmpty();editor->findChild<QPushButton*>("noteCreate")->click();
            });QTimer::singleShot(native?180000:15000,&page,[]{qFatal("File chooser timed out");});fill.start();page.findChild<QPushButton*>("noteAdd")->click();fill.stop();QVERIFY(retained);QCOMPARE(dept.catalog().size(),type=="file"?1:2);
        }
        for(auto e:dept.catalog()){const auto record=e.toObject();QVERIFY(!card(page,record["id"].toString())->expanded());QCOMPARE(QDir::fromNativeSeparators(record["target"].toString()),record["targetType"]=="file"?target:dir.path());}

    }
    void purgeDeletedSecretsOnUnlock(){
        QTemporaryDir dir;VaultRepository vault(dir.filePath("vault"));const auto salt=CryptoTool::random(16);auto key=CryptoTool::derive("test-master-password",salt);
        const QJsonArray old{QJsonObject{{"id","gone"},{"kind",0},{"title","已删除"},{"body","gone-secret"},{"deleted",true}},QJsonObject{{"id","keep"},{"kind",0},{"title","保留"},{"body","keep-secret"},{"tags","旧标签"}}};
        auto envelope=CryptoTool::encrypt(QJsonDocument(old).toJson(QJsonDocument::Compact),key);envelope["version"]=1;envelope["kdf"]="PBKDF2-SHA256-600000";envelope["salt"]=QString::fromLatin1(salt.toBase64());QVERIFY(vault.write(envelope));CryptoTool::wipe(key);
        NoteDepartment dept(&vault);QVERIFY(dept.unlock("test-master-password"));QVERIFY(dept.entry("gone").isEmpty());QVERIFY(!dept.entry("keep").contains("tags"));QCOMPARE(dept.entries().size(),1);dept.lock();QVERIFY(!QFile::exists(vault.path()+".before-split"));QVERIFY(dept.unlock("test-master-password"));QCOMPARE(dept.entries().size(),1);
    }
    void tagSelectionLimit(){
        QTemporaryDir dir;VaultRepository vault(dir.filePath("vault"));NoteDepartment dept(&vault);seed(dept,1);NoteWidget page(&dept);page.resize(900,740);page.show();QTest::qWait(20);auto c=card(page,"0");QTest::mouseDClick(c,Qt::LeftButton);
        auto fields=c->findChild<NoteFields*>();auto available=fields->findChild<QListWidget*>("noteAvailableTags");
        for(int i=0;i<2;++i){auto item=available->item(0);QTest::mouseClick(available->viewport(),Qt::LeftButton,Qt::NoModifier,available->visualItemRect(item).center());}
        QCOMPARE(fields->tags->selectedCount(),3);QCOMPARE(dept.entry("0")["tags"].toString().split(',').size(),3);QVERIFY(!(available->item(0)->flags()&Qt::ItemIsEnabled));
        auto selected=fields->findChild<QListWidget*>("noteSelectedTags");QTest::mouseClick(selected->viewport(),Qt::LeftButton,Qt::NoModifier,selected->visualItemRect(selected->item(0)).center());QCOMPARE(fields->tags->selectedCount(),2);QVERIFY(available->item(0)->flags()&Qt::ItemIsEnabled);
        auto added=available->item(0);const auto historical=added->data(Qt::UserRole).toString();QTest::mouseClick(available->viewport(),Qt::LeftButton,Qt::NoModifier,available->visualItemRect(added).center());
        const auto common=ConfigManager::getInstance()->noteTags();auto fewer=common;fewer.removeAll(historical);ConfigManager::getInstance()->setNoteTags(fewer);
        for(int i=0;i<selected->count();++i)if(selected->item(i)->data(Qt::UserRole)==historical){QTest::mouseClick(selected->viewport(),Qt::LeftButton,Qt::NoModifier,selected->visualItemRect(selected->item(i)).center());break;}
        bool found=false;for(int i=0;i<available->count();++i)if(available->item(i)->data(Qt::UserRole)==historical)found=true;QVERIFY(found);ConfigManager::getInstance()->setNoteTags(common);
    }
    void designSummariesAndEmptyStates(){
        QTemporaryDir dir;VaultRepository vault(dir.filePath("vault"));NoteDepartment dept(&vault);
        const QList<QPair<QString,QString>> papers{{"读书摘记","把注意力放在今天能够完成的一小步。\n周末整理这周的阅读记录。"},{"实验室的咖啡","小林喜欢不加糖的拿铁；周三小组讨论前可以顺路带上。"},{"答辩准备","先讲问题和结果，再补充方法。\n准备一份能在三分钟内讲完的版本。"},{"周末散步","图书馆旁边新开了一家书店，周六下午去看看。"},{"课程资料整理","按课程名归档讲义、笔记与作业，保留原始文件。"},{"一个很长的标题，用来检查卡片中标题最多两行并在末尾省略的效果","第一行\n第二行\n第三行\n第四行不应挤到 tag 的位置。"}};
        for(int i=0;i<papers.size();++i)QVERIFY(dept.save({{"id",QString("paper%1").arg(i)},{"kind",2},{"title",papers[i].first},{"body",papers[i].second},{"tags",i%2?"生活":"学习,收藏"}}));
        QVERIFY(dept.save({{"id","website"},{"kind",1},{"title","大学图书馆"},{"targetType","website"},{"target","https://library.example.edu/resources"},{"tags","学习"}}));
        QVERIFY(dept.save({{"id","file"},{"kind",1},{"title","课程论文"},{"targetType","file"},{"target",dir.filePath("课程论文.pdf")},{"tags","项目"}}));
        QVERIFY(dept.save({{"id","folder"},{"kind",1},{"title","本学期资料"},{"targetType","folder"},{"target",dir.path()},{"tags","学习,收藏"}}));
        QVERIFY(dept.create("test-master-password"));QVERIFY(dept.save({{"id","secret"},{"kind",0},{"title","校园服务账号"},{"body","PRIVATE-BODY-MUST-NOT-APPEAR"}}));dept.lock();
        NoteWidget page(&dept);page.resize(1180,760);page.show();QTest::mouseMove(&page,QPoint(1,1));QTest::qWait(50);auto tabs=page.findChild<QTabBar*>("noteTabs");auto search=page.findChild<QLineEdit*>("noteSearch");
        for(auto c:page.findChildren<NoteCard*>())QVERIFY(!c->property("selected").toBool());
        auto paper=card(page,"paper0");QCOMPARE(paper->findChild<QLabel*>("noteCardPreview")->text(),papers[0].second);snapshot(page,"note-design-papers");
        auto summary=paper->findChild<QWidget*>("noteCardSummary");QTest::mouseClick(summary,Qt::LeftButton);QVERIFY(paper->property("selected").toBool());QTest::mouseClick(summary,Qt::LeftButton);QVERIFY(!paper->property("selected").toBool());
        search->setText("not-found");QVERIFY(page.findChild<QPushButton*>("noteClearFilters")->isVisible());auto emptyAction=page.findChild<QPushButton*>("noteEmptyAction");QVERIFY(emptyAction->isVisible());QCOMPARE(emptyAction->text(),QString("清除筛选"));snapshot(page,"note-design-no-results");emptyAction->click();QVERIFY(search->text().isEmpty());QVERIFY(!paper->isHidden());
        tabs->setCurrentIndex(1);auto website=card(page,"website");QCOMPARE(website->findChild<QLabel*>("noteCardPreview")->text(),QString("library.example.edu"));QCOMPARE(card(page,"file")->findChild<QLabel*>("noteCardPreview")->text(),QString("课程论文.pdf"));snapshot(page,"note-design-bookmarks");
        tabs->setCurrentIndex(2);auto secret=card(page,"secret");QVERIFY(secret->findChild<QLabel*>("noteCardPreview")->isHidden());QVERIFY(secret->findChild<QLabel*>("noteCardPreview")->text().isEmpty());QVERIFY(secret->findChild<QWidget*>("noteLockMark"));QVERIFY(!secret->entry().contains("body"));snapshot(page,"note-design-secrets");
        tabs->setCurrentIndex(0);QTest::mouseDClick(paper,Qt::LeftButton);auto fields=paper->findChild<NoteFields*>();QVERIFY(fields);fields->body->setPlainText("保存状态检查");auto status=fields->findChild<QLabel*>("noteSaveStatus");QCOMPARE(status->text(),QString("等待保存…"));QTRY_COMPARE(status->text(),QString("已保存"));fields->title->clear();QVERIFY(!paper->save());QVERIFY(status->text().contains("保存失败"));fields->title->setText(papers[0].first);QVERIFY(paper->save());snapshot(page,"note-design-editor");QVERIFY(page.prepareLeave());
        page.resize(420,500);QTest::qWait(50);QVERIFY(page.width()<=420);page.findChild<QScrollArea*>("noteScroll")->verticalScrollBar()->setValue(0);snapshot(page,"note-design-narrow");
        bool fitted=false;QTimer::singleShot(50,&page,[&]{auto dialog=qobject_cast<QDialog*>(activeTestDialog());if(!dialog||dialog->objectName()!="noteEditor")return;fitted=dialog->width()<=page.width()&&dialog->height()<=page.height();const auto out=qEnvironmentVariable("ZTL_QA_DIR");if(!out.isEmpty())dialog->grab().save(out+"/note-design-create-narrow.png");dialog->reject();});
        page.findChild<QPushButton*>("noteAdd")->click();QVERIFY(fitted);
        QTemporaryDir emptyDir;VaultRepository emptyVault(emptyDir.filePath("vault"));NoteDepartment emptyDept(&emptyVault);NoteWidget emptyPage(&emptyDept);emptyPage.resize(900,700);emptyPage.show();QTest::qWait(30);QCOMPARE(emptyPage.findChild<QPushButton*>("noteEmptyAction")->text(),QString("新增纸条"));snapshot(emptyPage,"note-design-empty");
    }
};
QTEST_MAIN(NoteTests)
#include "test_note.moc"
