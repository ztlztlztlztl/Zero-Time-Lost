#include "settingdeadlinepage.h"

SettingDeadlinePage::SettingDeadlinePage(QWidget *parent)
    : QWidget{parent}
{
    qInfo() << "[SettingDeadlinePage] 启动。";

    m_deadlineBasical = ConfigManager::getInstance()->getDeadlineBasical();

    // ui初始化
    // 滚动区
    QVBoxLayout *generalLayout = new QVBoxLayout(this);
    generalLayout->setContentsMargins(0, 0, 0, 0);
    m_mainScroll = new QScrollArea(this);
    generalLayout->addWidget(m_mainScroll);
    QWidget *scrollContent = new QWidget();
    QVBoxLayout *contentLayout = new QVBoxLayout(scrollContent);
    // 标题
    m_lblTitle = new QLabel("--DDL--");
    contentLayout->addWidget(m_lblTitle);
    // 标题
    m_lblBasicalConfigTitle = new QLabel("-基础配置-");
    contentLayout->addWidget(m_lblBasicalConfigTitle);
    // past数量
    QHBoxLayout *initPastLayout = new QHBoxLayout();
    m_initPastText = new QLabel("开机过去DDL加载数");
    m_initPastNumber = new QLabel(QString::number(m_deadlineBasical.initialPastNumber));
    m_initPastInput = new QLineEdit();
    m_initPastInput->setPlaceholderText("修改处，最大 200");
    QRegularExpression re("^(?:[1-9]\\d?|1\\d\\d|200)$");
    QRegularExpressionValidator *validator = new QRegularExpressionValidator(re, this);
    m_initPastInput->setValidator(validator);
    m_initPastBtn = new QPushButton("修改");
    initPastLayout->addWidget(m_initPastText);
    initPastLayout->addWidget(m_initPastNumber);
    initPastLayout->addWidget(m_initPastInput);
    initPastLayout->addWidget(m_initPastBtn);
    contentLayout->addLayout(initPastLayout);
    // 保存时长
    QHBoxLayout *saveDurationLyt = new QHBoxLayout();
    m_saveDurationText = new QLabel("过去保存时长", this);
    m_saveDurationCombo = new NoWheelComboBox(this);
    for (const auto &item : ZTL::Setting::deadlinePage::saveDuration){
        m_saveDurationCombo->addItem(item.first, item.second);
    }
    int piIndex = m_saveDurationCombo->findData(m_deadlineBasical.saveDuration);
    if (piIndex != -1) {
        m_saveDurationCombo->setCurrentIndex(piIndex);
    }
    saveDurationLyt->addWidget(m_saveDurationText);
    saveDurationLyt->addWidget(m_saveDurationCombo);
    contentLayout->addLayout(saveDurationLyt);

    // 提醒时长
    QHBoxLayout *remindTimeLyt = new QHBoxLayout();
    m_remindTimeText = new QLabel("提醒时长", this);
    m_remindTimeCombo = new NoWheelComboBox(this);
    for (const auto &item : ZTL::Setting::deadlinePage::remindTime){
        m_remindTimeCombo->addItem(item.first, item.second);
    }
    piIndex = m_remindTimeCombo->findData(m_deadlineBasical.remindTime);
    if (piIndex != -1) {
        m_remindTimeCombo->setCurrentIndex(piIndex);
    }
    remindTimeLyt->addWidget(m_remindTimeText);
    remindTimeLyt->addWidget(m_remindTimeCombo);
    contentLayout->addLayout(remindTimeLyt);

    // 常用tag
    QHBoxLayout *commonTagLayout = new QHBoxLayout();
    m_lblCommonTagTest = new QLabel("常用 tag");
    m_lblCommonTagTest->setAlignment(Qt::AlignTop);
    commonTagLayout->addWidget(m_lblCommonTagTest);
    QVBoxLayout *rightLayout = new QVBoxLayout();
    // 四个按钮
    QHBoxLayout *btnsLayout = new QHBoxLayout();
    m_upTagBtn = new QPushButton("up");
    m_downTagBtn = new QPushButton("down");
    m_deleteTagBtn = new QPushButton("delete");
    btnsLayout->addWidget(m_upTagBtn);
    btnsLayout->addWidget(m_downTagBtn);
    btnsLayout->addWidget(m_deleteTagBtn);
    rightLayout->addLayout(btnsLayout);
    // 列表区
    QWidget *commomTagContent = new QWidget(); // 创建内容载体
    m_contentTagLayout = new QVBoxLayout(commomTagContent);
    m_contentTagLayout->setAlignment(Qt::AlignTop);
    // 组装
    rightLayout->addWidget(commomTagContent);
    commonTagLayout->addLayout(rightLayout);
    contentLayout->addLayout(commonTagLayout);
    m_clickedTagIndex = -1;

    // 添加tag
    QHBoxLayout *addCityLayout = new QHBoxLayout();
    m_lblAddTagTest = new QLabel("添加新 tag：", this);
    m_AddTagInput = new QLineEdit(this);
    QRegularExpression _re(R"(^(?:[\x{4e00}-\x{9fa5}_]{1,4}|[a-zA-Z_]{1,15})$)");
    QRegularExpressionValidator *_validator = new QRegularExpressionValidator(_re, this);
    m_AddTagInput->setValidator(_validator);
    m_AddTagInput->setPlaceholderText("描述，不超过4个汉字或15个字母，特殊字符仅下划线");
    m_AddTagSubmit = new QPushButton("提交", this);
    addCityLayout->addWidget(m_lblAddTagTest);
    addCityLayout->addWidget(m_AddTagInput);
    addCityLayout->addWidget(m_AddTagSubmit);
    contentLayout->addLayout(addCityLayout);



    // 收尾
    m_mainScroll->setWidget(scrollContent);
    m_mainScroll->setWidgetResizable(true);

    resetUI();

    connect(m_initPastBtn, &QPushButton::clicked, this, [=](){
        if (m_initPastInput->text().isEmpty()) return;
        m_deadlineBasical.initialPastNumber = m_initPastInput->text().toInt();
        qDebug() << "[SettingDeadlinePage] 过往上限改为：" << m_deadlineBasical.initialPastNumber;
        ConfigManager::getInstance()->setDeadlineBasical(m_deadlineBasical);
        m_initPastNumber->setText(QString::number(m_deadlineBasical.initialPastNumber));
        m_initPastInput->clear();
    });

    connect(m_saveDurationCombo, &QComboBox::currentIndexChanged, this, [=](int index){
        if (index < 0) return;
        int data = m_saveDurationCombo->itemData(index).toInt();
        qDebug() << "[SettingDeadlinePage] 保存时长改为：" << data << " 秒";
        m_deadlineBasical.saveDuration = data;
        ConfigManager::getInstance()->setDeadlineBasical(m_deadlineBasical);
    });

    connect(m_remindTimeCombo, &QComboBox::currentIndexChanged, this, [=](int index){
        if (index < 0) return;
        int data = m_remindTimeCombo->itemData(index).toInt();
        qDebug() << "[SettingDeadlinePage] 提醒时长改为：" << data << " 秒";
        m_deadlineBasical.remindTime = data;
        ConfigManager::getInstance()->setDeadlineBasical(m_deadlineBasical);
    });

    connect(m_AddTagSubmit, &QPushButton::clicked, this, [=](){
        if (m_AddTagInput->text().isEmpty()) return;
        m_deadlineBasical.commonTags.append(m_AddTagInput->text());
        qDebug() << "[SettingDeadlinePage] 添加常用 tag：" << m_AddTagInput->text();
        ConfigManager::getInstance()->setDeadlineBasical(m_deadlineBasical);
        m_AddTagInput->clear();
        resetUI();
    });

    // up
    connect(m_upTagBtn, &QPushButton::clicked, this, [=](){
        if (m_clickedTagIndex == 0 || m_clickedTagIndex == -1){
            qDebug() << "[SettingDeadlinePage] 当前选定 tag 无法上行。";
            return;
        }
        int rawIndex = m_clickedTagIndex;
        int tarIndex = m_clickedTagIndex - 1;
        m_clickedTagIndex -= 1;
        auto tmp = m_deadlineBasical.commonTags[rawIndex];
        m_deadlineBasical.commonTags[rawIndex] = m_deadlineBasical.commonTags[tarIndex];
        m_deadlineBasical.commonTags[tarIndex] = tmp;
        ConfigManager::getInstance()->setDeadlineBasical(m_deadlineBasical);
        resetUI();
    });
    // down
    connect(m_downTagBtn, &QPushButton::clicked, this, [=](){
        if (m_clickedTagIndex == m_deadlineBasical.commonTags.length() - 1 || m_clickedTagIndex == -1){
            qDebug() << "[SettingDeadlinePage] 当前选定 tag 无法下行。";
            return;
        }
        int rawIndex = m_clickedTagIndex;
        int tarIndex = m_clickedTagIndex + 1;
        m_clickedTagIndex += 1;
        auto tmp = m_deadlineBasical.commonTags[rawIndex];
        m_deadlineBasical.commonTags[rawIndex] = m_deadlineBasical.commonTags[tarIndex];
        m_deadlineBasical.commonTags[tarIndex] = tmp;
        ConfigManager::getInstance()->setDeadlineBasical(m_deadlineBasical);
        resetUI();
    });
    // delete
    connect(m_deleteTagBtn, &QPushButton::clicked, this, [=](){
        if (m_clickedTagIndex == -1){
            qDebug() << "[SettingDeadlinePage] 未选择，不可执行删除";
            return;
        }
        m_deadlineBasical.commonTags.remove(m_clickedTagIndex);
        m_clickedTagIndex = -1;
        ConfigManager::getInstance()->setDeadlineBasical(m_deadlineBasical);
        resetUI();
    });









}

void SettingDeadlinePage::resetUI(){
    qInfo() << "[SettingDeadlinePage] 刷新setting页的DDL页的UI。";

    if (m_contentTagLayout) {
        while (m_contentTagLayout->count() > 0) {
            QLayoutItem *item = m_contentTagLayout->takeAt(0);
            if (QWidget *widget = item->widget()) {
                widget->deleteLater();
            }
            delete item;
        }
    }

    // 删除旧按钮组
    if (QButtonGroup *oldCityGroup = this->findChild<QButtonGroup*>("tagGroup")) {
        delete oldCityGroup;
    }
    QButtonGroup *tagBtnGroup = new QButtonGroup(this);
    tagBtnGroup->setObjectName("tagGroup");
    tagBtnGroup->setExclusive(true);

    if(m_deadlineBasical.commonTags.isEmpty()){
        QLabel *holder = new QLabel("尚未添加常用 tag");
        m_contentTagLayout->addWidget(holder);
    }
    else{
        for (int i = 0; i < m_deadlineBasical.commonTags.length(); i++) {
            const QString &tag = m_deadlineBasical.commonTags[i];
            QPushButton *cityBtn = new QPushButton(tag);
            cityBtn->setCheckable(true);
            if(i == m_clickedTagIndex){
                cityBtn->setChecked(true);
            }
            m_contentTagLayout->addWidget(cityBtn);
            tagBtnGroup->addButton(cityBtn, i);
        }
    }
    connect(tagBtnGroup, &QButtonGroup::idClicked, this, [=](int id){
        qDebug() << "[SettingDeadlinePage] 切换了当前点击的 tag，索引为：" << id;
        m_clickedTagIndex = id;
    });
}








