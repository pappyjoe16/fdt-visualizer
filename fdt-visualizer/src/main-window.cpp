#include "main-window.hpp"
#include "ui_main-window.h"

#include <iostream>
#include <string>
#include <cstdlib>

#include <QAction>
#include <QByteArray>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QMessageBox>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QMouseEvent>
#include <QEvent>
#include <QProcess>
#include <QString>
#include <QTextStream>

#include <dialogs.hpp>
#include <endian-conversions.hpp>
#include <fdt/fdt-parser.hpp>
#include <fdt/fdt-view.hpp>
#include <menu-manager.hpp>
#include <viewer-settings.hpp>

#include "submodules/qhexview/model/buffer/qmemorybuffer.h"
#include "submodules/qhexview/qhexview.h"

using namespace Window;

MainWindow::MainWindow(QWidget *parent)
        : QMainWindow(parent)
        , m_ui(std::make_unique<Ui::MainWindow>()) {
    setWindowIcon(QIcon(":/resources/fdt-viewer.svg"));
    m_ui->setupUi(this);
    m_ui->treeWidget->setSelectionMode(QTreeWidget::ExtendedSelection);
    //m_ui->treeWidget->setSelectionBehavior(QTreeWidget::SelectRows);

    m_ui->preview->setCurrentWidget(m_ui->text_view_page);
    m_ui->splitter->setEnabled(false);

    m_viewer = std::make_unique<fdt::viewer>(m_ui->treeWidget);

    m_hexview = new QHexView();
    m_hexview->setReadOnly(true);
    m_ui->hexview_layout->addWidget(m_hexview);

    m_ui->splitter->setStretchFactor(0, 2);
    m_ui->splitter->setStretchFactor(1, 5);

    m_menu = std::make_unique<menu_manager>(m_ui->menubar);
    connect(m_menu.get(), &menu_manager::show_full_screen, this, &MainWindow::showFullScreen);
    connect(m_menu.get(), &menu_manager::show_normal, this, &MainWindow::showNormal);
    connect(m_menu.get(), &menu_manager::quit, this, &MainWindow::close);

    m_ui->text_view->setWordWrapMode(QTextOption::NoWrap);

    connect(m_menu.get(), &menu_manager::use_word_wrap, [this](const bool value) {
        m_ui->text_view->setWordWrapMode(value ? QTextOption::WordWrap : QTextOption::NoWrap);
    });

    connect(m_menu.get(), &menu_manager::show_about_qt, []() { QApplication::aboutQt(); });

    connect(m_menu.get(), &menu_manager::open_file, this, [this]() {
        fdt::open_file_dialog(this, [this](auto &&...values) { open_file(std::forward<decltype(values)>(values)...); });
    });

    connect(m_menu.get(), &menu_manager::open_directory, this, [this]() {
        fdt::open_directory_dialog(this, [this](auto &&...values) { open_directory(std::forward<decltype(values)>(values)...); });
    });
    connect(m_ui->load_dtb2, &QPushButton::clicked, this, [this]() {
        fdt::open_file_dialog(this, [this](auto &&...values) { open_file(std::forward<decltype(values)>(values)...); });
    });

    connect(m_ui->quick_search, &QLineEdit::textEdited, this, [this](const QString &text) {
        auto node = m_ui->treeWidget->invisibleRootItem();

        fdt::fdt_content_filter(
            node, [&text](const string &value) -> bool {
                if (text.isEmpty())
                    return true;

                return value.indexOf(text) != -1;
            });

        update_view();
    });

    connect(m_menu.get(), &menu_manager::quit, this, &MainWindow::close);
    connect(m_menu.get(), &menu_manager::close, this, [this]() {
        if (m_fdt) {
            delete m_fdt;
            m_fdt = nullptr;
            update_view();
        }
    });

    connect(m_menu.get(), &menu_manager::property_export, this, &MainWindow::property_export);

    connect(m_menu.get(), &menu_manager::close_all, this, [this]() {
        m_fdt = nullptr;
        m_ui->treeWidget->clear();
        update_view();
    });

    connect(m_ui->treeWidget, &QTreeWidget::itemSelectionChanged, this, &MainWindow::update_view);

    viewer_settings settings;
    m_ui->text_view->setWordWrapMode(settings.view_word_wrap.value() ? QTextOption::WordWrap : QTextOption::NoWrap);

    if (settings.window_show_fullscreen.value())
        showFullScreen();

    auto rect = settings.window_position.value();

    if (!rect.isEmpty())
        setGeometry(rect);
}

QString MainWindow::run_dtdiff(const QString& file1, const QString& file2) {
    QProcess process;

    QStringList arguments;
    arguments << file1 << file2;

    process.start("dtdiff", arguments);
    if (!process.waitForFinished()) {
        return QString("Error: dtdiff command failed to run or did not finish.");
    }
    QString output = process.readAllStandardOutput();

    return output;
}

void MainWindow::writeToTempFile(const QString& content, QTemporaryFile& tempFile) {
    if (tempFile.open()) {
        QTextStream out(&tempFile);
        out << content;
        out.flush();
        tempFile.close();
        qDebug() << "Written to:" << tempFile.fileName();
    } else {
        qWarning() << "Failed to open temporary file.";
    }
}

QString MainWindow::run_nodediff(const QString& node1, const QString& node2) {
    
    QTemporaryFile tempFile1(QDir::tempPath() + "/nodefile1XXXXXX.txt");
    tempFile1.setAutoRemove(false);
    writeToTempFile(node1, tempFile1);

    QTemporaryFile tempFile2(QDir::tempPath() + "/nodefile2XXXXXX.txt");
    tempFile2.setAutoRemove(false);
    writeToTempFile(node2, tempFile2);

    QProcess process;

    QStringList arguments;
    arguments << QString("-u")  << QString("-a") << QFileInfo(tempFile1).absoluteFilePath() << QFileInfo(tempFile2).absoluteFilePath();
    
    process.start("diff", arguments);
    if (!process.waitForFinished()) {
        return QString("Error: diff command failed to run or did not finish.");
    }
    QString output = process.readAllStandardOutput();
    
    return output;
}

MainWindow::~MainWindow() {
    viewer_settings settings;
    settings.window_position.set(geometry());
}

void MainWindow::open_directory(const string &path) {
    QDirIterator iter(path, {"*.dtb", "*.dtbo"}, QDir::Files);
    while (iter.hasNext()) {
        iter.next();
        open_file(iter.filePath());
    }
}

void MainWindow::open_file(const string &path) {
    if (!open(path))
        dialogs::warn_invalid_fdt(path, this);
}

bool MainWindow::open(const string &path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return false;

    const auto info = file_info(path);

    if (m_viewer->is_loaded(info.absoluteFilePath()) &&
        dialogs::ask_already_opened(this))
        return true;

    const auto ret = m_viewer->load(file.readAll(), info.fileName(), info.absoluteFilePath());
    update_view();
    return ret;
}

void MainWindow::update_fdt_path(QTreeWidgetItem *item) {
    m_menu->set_close_enabled(item);

    if (nullptr == item) {
        m_ui->path->clear();
        return;
    }

    QString path = item->text(0);
    auto root = item;
    while (root->parent()) {
        path = root->parent()->text(0) + "/" + path;
        root = root->parent();
    }

    m_fdt = root;

    // m_ui->statusbar->showMessage("file://" + root->data(0, QT_ROLE_FILEPATH).toString());
    m_ui->path->setText("fdt://" + path);
}

constexpr auto VIEW_TEXT_CACHE_SIZE = 1024 * 1024;

void MainWindow::update_view() {
    m_ui->splitter->setEnabled(m_ui->treeWidget->topLevelItemCount());
    m_menu->set_close_enabled(!m_ui->treeWidget->selectedItems().isEmpty());
    m_menu->set_close_all_enabled(m_ui->treeWidget->topLevelItemCount());

    if (m_ui->treeWidget->selectedItems().isEmpty()) {
        m_ui->preview->setCurrentWidget(m_ui->text_view_page);
        m_ui->text_view->clear();
        m_ui->statusbar->clearMessage();
        m_ui->path->clear();
        return;
    }

    const auto item = m_ui->treeWidget->selectedItems().first();
    m_ui->path_2->clear();

    if (m_ui->treeWidget->selectedItems().size() == 2) {
        m_ui->text_view->clear();
        m_ui->statusbar->clearMessage();
        m_ui->path->clear();
        
        QString first_Item = m_ui->treeWidget->selectedItems().first()->text(0);
        QString second_Item = m_ui->treeWidget->selectedItems().last()->text(0);
        m_ui->path_2->setText("fdt://" + second_Item);
        
        m_ui->statusbar->showMessage("Comparing " + first_Item + " and " + second_Item);
        
        
    }
    else {
        m_ui->statusbar->showMessage("fdt://" + item->text(0));
    }
    
    const auto type = item->data(0, QT_ROLE_NODETYPE).value<NodeType>();
    m_ui->preview->setCurrentWidget(NodeType::Node == type ? m_ui->text_view_page : m_ui->property_view_page);

    if (NodeType::Property == type) {
        const auto property = item->data(0, QT_ROLE_PROPERTY).value<fdt_property>();
        m_hexview->setDocument(QHexDocument::fromMemory<QMemoryBuffer>(property.data));
    }

    m_ui->text_view->clear();
    update_fdt_path(item);

    string ret;
    ret.reserve(VIEW_TEXT_CACHE_SIZE);

    fdt::fdt_view_dts(item, ret);
    if (m_ui->treeWidget->selectedItems().size() == 2){
        const auto item1 = m_ui->treeWidget->selectedItems().first();
        const auto item2 = m_ui->treeWidget->selectedItems().last();

        const auto path1 = item1->data(0, QT_ROLE_FILEPATH).toString();
        const auto path2 = item2->data(0, QT_ROLE_FILEPATH).toString();
        QString diff_out;

        if (path1.contains(".dtb") && path2.contains(".dtb")) {
            diff_out = run_dtdiff(QString(m_ui->treeWidget->selectedItems().first()->data(0, QT_ROLE_FILEPATH).toString()), QString(m_ui->treeWidget->selectedItems().last()->data(0, QT_ROLE_FILEPATH).toString()));
        } else {
            string ret1;
            ret1.reserve(VIEW_TEXT_CACHE_SIZE);
            fdt::fdt_view_dts(item1, ret1);

            string ret2;
            ret2.reserve(VIEW_TEXT_CACHE_SIZE);
            fdt::fdt_view_dts(item2, ret2);

            diff_out = run_nodediff(QString(ret1), QString(ret2));
        }
        
        m_ui->text_view->setText(diff_out);
    }
    else{
        m_ui->text_view->setText(ret);
    }
    
}

void MainWindow::property_export() {
    if (m_ui->treeWidget->selectedItems().isEmpty())
        return;

    const auto item = m_ui->treeWidget->selectedItems().first();
    const auto type = item->data(0, QT_ROLE_NODETYPE).value<NodeType>();

    if (NodeType::Property == type) {
        const auto property = item->data(0, QT_ROLE_PROPERTY).value<fdt_property>();
        m_hexview->setDocument(QHexDocument::fromMemory<QMemoryBuffer>(property.data));
        fdt::export_property_file_dialog(this, property.data, property.name);
    }
}
