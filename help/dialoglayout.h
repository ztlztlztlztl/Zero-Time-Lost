#pragma once
#include <QtWidgets>

namespace DialogLayout {
inline QRect fit(QDialog &dialog,QWidget *owner,QSize preferred){
    const auto screen=owner->screen()->availableGeometry();
    auto bounds=QRect(owner->window()->mapToGlobal(QPoint()),owner->window()->size()).intersected(screen).adjusted(12,12,-12,-12);
    if(bounds.width()<240||bounds.height()<200)bounds=screen.adjusted(12,12,-12,-12);
    const auto required=preferred.expandedTo(dialog.minimumSizeHint()).expandedTo(dialog.minimumSize());
    if(!dialog.property("ztlScrollWrapped").toBool()&&(required.width()>bounds.width()||required.height()>bounds.height())){
        // Keep every field reachable even when its native minimum exceeds the work area.
        auto content=new QWidget;content->setLayout(dialog.layout());
        auto layout=new QVBoxLayout(&dialog);layout->setContentsMargins(0,0,0,0);
        auto scroll=new QScrollArea;scroll->setWidgetResizable(true);scroll->setWidget(content);layout->addWidget(scroll);
        dialog.setMinimumSize(0,0);
        dialog.setProperty("ztlScrollWrapped",true);
    }
    dialog.setMaximumSize(bounds.size());dialog.resize(required.boundedTo(bounds.size()));
    return bounds;
}
inline QPoint clamp(QPoint position,const QSize &size,const QRect &bounds){
    position.setX(qBound(bounds.left(),position.x(),qMax(bounds.left(),bounds.right()-size.width()+1)));
    position.setY(qBound(bounds.top(),position.y(),qMax(bounds.top(),bounds.bottom()-size.height()+1)));
    return position;
}
}
