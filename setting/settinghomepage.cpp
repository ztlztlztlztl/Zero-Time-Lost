#include "settinghomepage.h"
#include "settingcontrols.h"
#include <QtWidgets>
SettingHomePage::SettingHomePage(QWidget *parent):QWidget(parent){
    auto layout=new QVBoxLayout(this);layout->setContentsMargins(0,0,0,0);layout->setSpacing(14);
    auto row=new QWidget;auto form=SettingControls::form(row);current=new QLabel;form->addRow(SettingControls::label("当前城市"),current);layout->addWidget(row);
    layout->addWidget(SettingControls::label("收藏的城市"));
    cities=new SettingsListWidget;cities->setObjectName("savedCities");SettingControls::configureList(cities);layout->addWidget(cities);
    auto actions=new QHBoxLayout;
    for(const auto &name:{"上移","下移","设为当前","修改","删除","添加"}){
        auto button=new SettingButton(name);button->setObjectName("city"+QString(name));actions->addWidget(SettingControls::buttonSlot(button,true));
        connect(button,&QPushButton::clicked,this,[this,name]{
            const auto action=QString(name);if(action=="添加"||action=="修改"){editCity(action=="添加");return;}
            auto config=ConfigManager::getInstance()->getHomeweather();const int row=cities->currentRow();if(row<0||row>=config.savedCities.size())return;
            int selection=row;
            if(action=="删除"){
                config.savedCities.removeAt(row);
                if(config.currentCityIndex==row)config.currentCityIndex=-1;else if(config.currentCityIndex>row)--config.currentCityIndex;
                selection=qMin(row,int(config.savedCities.size())-1);
            }else if(action=="设为当前")config.currentCityIndex=row;
            else{
                const int target=row+(action=="上移"?-1:1);if(target<0||target>=config.savedCities.size())return;
                config.savedCities.swapItemsAt(row,target);selection=target;
                if(config.currentCityIndex==row)config.currentCityIndex=target;else if(config.currentCityIndex==target)config.currentCityIndex=row;
            }
            ConfigManager::getInstance()->setHomeweather(config);cities->setCurrentRow(selection);
        });
    }
    layout->addLayout(actions);connect(ConfigManager::getInstance(),&ConfigManager::homeWeatherChanged,this,&SettingHomePage::resetUI);resetUI();
}
void SettingHomePage::resetUI(){
    const auto config=ConfigManager::getInstance()->getHomeweather();const int selected=cities->currentRow();cities->clear();
    for(int i=0;i<config.savedCities.size();++i){const auto city=config.savedCities[i];auto item=new QListWidgetItem(city.name+" ("+city.code+")"+(i==config.currentCityIndex?" · 当前":""),cities);item->setData(Qt::UserRole,city.code);}
    current->setText(config.currentCityIndex>=0&&config.currentCityIndex<config.savedCities.size()?config.savedCities[config.currentCityIndex].name:"未选择");
    cities->setCurrentRow(selected>=0&&selected<cities->count()?selected:-1);
}
void SettingHomePage::editCity(bool creating){
    const auto config=ConfigManager::getInstance()->getHomeweather();const int row=cities->currentRow();if(!creating&&(row<0||row>=config.savedCities.size()))return;
    const auto original=creating?weatherCity{}:config.savedCities[row];
    auto popup=SettingControls::popup(this,creating?"添加城市":"修改城市","cityPopup");auto layout=qobject_cast<QVBoxLayout*>(popup->layout());
    auto fields=new QWidget;auto form=SettingControls::form(fields);auto name=new QLineEdit(original.name);name->setObjectName("cityName");
    auto code=new QLineEdit(original.code);code->setObjectName("cityCode");code->setPlaceholderText("6 位高德城市编码");code->setValidator(new QRegularExpressionValidator(QRegularExpression("[0-9]{6}"),code));
    form->addRow(SettingControls::label("城市名"),name);form->addRow(SettingControls::label("城市编码"),code);layout->addWidget(fields);
    auto error=new QLabel;error->setWordWrap(true);layout->addWidget(error);auto save=new SettingButton(creating?"添加":"应用");save->setObjectName("cityApply");layout->addWidget(SettingControls::buttonSlot(save));
    connect(save,&QPushButton::clicked,popup,[=]{
        if(name->text().trimmed().isEmpty()||!code->hasAcceptableInput()){error->setText("请输入城市名及 6 位城市编码");return;}
        auto updated=ConfigManager::getInstance()->getHomeweather();int index=-1;
        for(int i=0;i<updated.savedCities.size();++i){const auto city=updated.savedCities[i];if(!creating&&city.code==original.code)index=i;
            if((creating||city.code!=original.code)&&(city.name==name->text().trimmed()||city.code==code->text())){error->setText("城市名或编码已存在");return;}}
        if(creating)updated.savedCities.append({name->text().trimmed(),code->text()});
        else if(index>=0)updated.savedCities[index]={name->text().trimmed(),code->text()};else {error->setText("原城市已被移除，请重新添加");return;}
        ConfigManager::getInstance()->setHomeweather(updated);popup->close();
    });
    popup->setMinimumWidth(460);SettingControls::showPopup(popup,cities);name->setFocus();
}
