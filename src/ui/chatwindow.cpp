#include "chatwindow.h"
#include "ui_chatwindow.h"

#include "QFrame"

ChatWindow::ChatWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::ChatWindow)
{
    ui->setupUi(this);
    // ui->Sidebar->setFrameShape(QFrame::Box);
    // ui->Sidebar->setFrameStyle(QFrame::StyledPanel | QFrame::Sunken);
    // ui->Sidebar->setLineWidth(600);
    // ui->Sidebar->setMidLineWidth(1);
    ui->Sidebar->setFixedWidth(60);
    ui->Topbar->setFixedHeight(60);
    ui->Right_Menu->setFixedWidth(220);
}

ChatWindow::~ChatWindow()
{
    delete ui;
}

void ChatWindow::on_btnRightMenuCollapse_clicked()
{
    if(ui->Right_Menu->isVisible())
        ui->Right_Menu->hide();
    else
        ui->Right_Menu->show();
}

