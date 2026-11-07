#include <QtTest>
#include <QtWidgets>
#include "notedepartment.h"
#include "cryptotool.h"
class VaultTests:public QObject {
    Q_OBJECT
private slots:
    void recoveryDraftSurvivesRestartBackupAndRekey(){
        QTemporaryDir dir;VaultRepository repo(dir.filePath("vault"));NoteDepartment notes(&repo);
        QVERIFY(notes.create("draft-master-password"));
        QVERIFY(notes.save({{"id","secret"},{"kind",0},{"title","正式标题"},{"body","上次保存的内容"}}));
        auto draft=notes.entry("secret");draft["title"]="";draft["body"]="unsaved-private-input-314159";
        QVERIFY(!notes.save(draft));QVERIFY2(notes.preserveDraft(draft),qPrintable(notes.error()));
        QCOMPARE(notes.entry("secret")["body"].toString(),QString("上次保存的内容"));
        QFile file(repo.path());QVERIFY(file.open(QIODevice::ReadOnly));QVERIFY(!file.readAll().contains("unsaved-private-input-314159"));file.close();notes.lock();
        NoteDepartment reopened(&repo);QVERIFY(reopened.recoveryDraft("secret").isEmpty());QVERIFY(!reopened.unlock("wrong-password"));
        QVERIFY(reopened.unlock("draft-master-password"));QCOMPARE(reopened.recoveryDraft("secret"),draft);
        const QJsonValue sealedDrafts=repo.read().value("drafts");QVERIFY(reopened.changePassword("new-draft-master-password"));QCOMPARE(repo.read()["drafts"],sealedDrafts);
        reopened.lock();QVERIFY(!reopened.unlock("draft-master-password"));QVERIFY(reopened.unlock("new-draft-master-password"));QCOMPARE(reopened.recoveryDraft("secret"),draft);
        QVERIFY(reopened.backup(dir.filePath("backup")));
        VaultRepository other(dir.filePath("restored"));NoteDepartment restored(&other);
        QVERIFY(restored.restore(dir.filePath("backup"),"new-draft-master-password"));QCOMPARE(restored.recoveryDraft("secret"),draft);
        auto fixed=draft;fixed["title"]="修正标题";QVERIFY(restored.save(fixed));QVERIFY(restored.recoveryDraft("secret").isEmpty());QVERIFY(other.read()["drafts"].toArray().isEmpty());
        QVERIFY(restored.preserveDraft(draft));QVERIFY(restored.remove("secret"));QVERIFY(restored.recoveryDraft("secret").isEmpty());QVERIFY(other.read()["drafts"].toArray().isEmpty());
        auto damaged=repo.read();auto drafts=damaged["drafts"].toArray();auto record=drafts[0].toObject();record["id"]="tampered";drafts[0]=record;damaged["drafts"]=drafts;QVERIFY(repo.write(damaged));
        QVERIFY(reopened.recoveryDraft("tampered").isEmpty());QVERIFY(reopened.error().contains("损坏"));QCOMPARE(reopened.entry("secret")["body"].toString(),QString("上次保存的内容"));
    }
    void recoveryDraftFallsBackToEncryptedMemory(){
        QTemporaryDir dir;const auto path=dir.filePath("vault");VaultRepository repo(path);NoteDepartment notes(&repo);
        QVERIFY(notes.create("fallback-master-password"));QVERIFY(notes.save({{"id","secret"},{"kind",0},{"title","正式标题"},{"body","original"}}));
        auto draft=notes.entry("secret");draft["title"]="";draft["body"]="fallback-private-text";
        QVERIFY(QFile::rename(path,path+".saved"));QVERIFY(QDir().mkdir(path));
        QVERIFY(!notes.preserveDraft(draft));QVERIFY(notes.error().contains("加密内存"));notes.lock();
        QVERIFY(!notes.backup(dir.filePath("incomplete-backup")));QVERIFY(!QFile::exists(dir.filePath("incomplete-backup")));
        QVERIFY(QDir().rmdir(path));QVERIFY(QFile::rename(path+".saved",path));
        QVERIFY(notes.unlock("fallback-master-password"));QCOMPARE(notes.recoveryDraft("secret"),draft);
        QVERIFY(notes.backup(dir.filePath("complete-backup")));notes.lock();
        NoteDepartment reopened(&repo);QVERIFY(reopened.unlock("fallback-master-password"));QCOMPARE(reopened.recoveryDraft("secret"),draft);
        // An unrelated vault must never receive this instance's fallback ciphertext.
        QVERIFY(QFile::rename(path,path+".saved"));QVERIFY(QDir().mkdir(path));draft["body"]="latest-private-text";QVERIFY(!reopened.preserveDraft(draft));
        QVERIFY(QDir().rmdir(path));VaultRepository foreign(dir.filePath("foreign"));NoteDepartment other(&foreign);QVERIFY(other.create("foreign-master-password"));
        QVERIFY(repo.write(foreign.read()));const auto before=repo.read();QVERIFY(!reopened.backup(dir.filePath("wrong-vault-backup")));QCOMPARE(repo.read(),before);
        QVERIFY(repo.write(VaultRepository(path+".saved").read()));
    }
    void lockedCreationBackupAndRekey(){
        QTemporaryDir dir;VaultRepository repo(dir.filePath("vault"));NoteDepartment notes(&repo);QVERIFY(notes.create("portable-master-password"));notes.lock();QVERIFY(notes.creationReady());
        const QString body=QString(10000,QChar('x'))+"private-value-978";QVERIFY(notes.save({{"id","unopened"},{"kind",0},{"title","标题"},{"body",body}}));QVERIFY(!notes.unlocked());QVERIFY(notes.entries().isEmpty());QVERIFY(!notes.entry("unopened").contains("body"));
        QFile f(repo.path());QVERIFY(f.open(QIODevice::ReadOnly));QVERIFY(!f.readAll().contains("private-value-978"));f.close();QVERIFY(notes.backup(dir.filePath("backup")));
        VaultRepository other(dir.filePath("another-machine/vault"));NoteDepartment restored(&other);QVERIFY2(restored.restore(dir.filePath("backup"),"portable-master-password"),qPrintable(restored.error()));QCOMPARE(restored.entry("unopened")["body"].toString(),body);
        const QJsonValue sealed=other.read().value("entries");QVERIFY(restored.changePassword("replacement-master-password"));QCOMPARE(other.read()["entries"],sealed);restored.lock();QVERIFY(!restored.unlock("portable-master-password"));QVERIFY(restored.unlock("replacement-master-password"));QCOMPARE(restored.entry("unopened")["body"].toString(),body);restored.lock();
        auto original=other.read();
        for(const QString &field:{"private","publicKey","metadata","payload","wrappedKey"}){
            auto modified=original;
            if(field=="publicKey"){auto pub=QByteArray::fromBase64(modified[field].toString().toLatin1());pub[pub.size()-1]^=1;modified[field]=QString::fromLatin1(pub.toBase64());}
            else if(field=="private"){auto data=modified[field].toObject();data["tag"]="broken";modified[field]=data;}
            else{auto records=modified["entries"].toArray();auto e=records[0].toObject();if(field=="metadata")e["id"]="different-id";else if(field=="wrappedKey")e[field]="broken";else{auto payload=e[field].toObject();payload["tag"]="broken";e[field]=payload;}records[0]=e;modified["entries"]=records;}
            QVERIFY(other.write(modified));QVERIFY(!restored.unlock("replacement-master-password"));QVERIFY(!restored.unlocked());QCOMPARE(other.read(),modified);
        }
        QVERIFY(other.write(original));QVERIFY(restored.unlock("replacement-master-password"));restored.lock();
        auto before=repo.read();QVERIFY(!notes.restore(dir.filePath("backup"),"wrong-password"));QCOMPARE(repo.read(),before);
    }
    void legacyUpgradeIsValidated(){
        QTemporaryDir dir;VaultRepository repo(dir.filePath("vault"));const auto salt=CryptoTool::random(16);auto key=CryptoTool::derive("legacy-password",salt);
        const QJsonArray old{QJsonObject{{"id","legacy"},{"kind",0},{"title","旧秘密"},{"body","old-private-content"},{"tags","old tag"}}};auto envelope=CryptoTool::encrypt(QJsonDocument(old).toJson(QJsonDocument::Compact),key);CryptoTool::wipe(key);envelope["version"]=1;envelope["kdf"]="PBKDF2-SHA256-600000";envelope["salt"]=QString::fromLatin1(salt.toBase64());QVERIFY(repo.write(envelope));
        NoteDepartment notes(&repo);QVERIFY(!notes.creationReady());QVERIFY(!notes.save({{"kind",0},{"title","未升级时禁止新增"},{"body","secret"}}));QCOMPARE(repo.read(),envelope);QVERIFY(!notes.unlock("wrong-password"));QCOMPARE(repo.read(),envelope);
        QVERIFY2(notes.unlock("legacy-password"),qPrintable(notes.error()));QCOMPARE(repo.read()["version"].toInt(),2);QCOMPARE(notes.entry("legacy")["body"].toString(),QString("old-private-content"));QVERIFY(!notes.entry("legacy").contains("tags"));notes.lock();QVERIFY(notes.save({{"id","new"},{"kind",0},{"title","新增"},{"body","new-private-content"}}));QVERIFY(notes.unlock("legacy-password"));QCOMPARE(notes.entries().size(),2);
    }
};
QTEST_MAIN(VaultTests)
#include "test_vault.moc"
