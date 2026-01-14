#pragma once

#include <QDialog>
#include <QVBoxLayout>
#include <QLabel>
#include <QRadioButton>
#include <QPushButton>
#include <QButtonGroup>
#include <QScrollArea>
#include <vector>

class ConfigManager;
struct RealmlistEntry;

/**
 * @brief Window for managing server/realmlist entries
 * 
 * Allows users to:
 * - View all configured servers
 * - Add, edit, delete servers
 * - Switch between servers
 */
class RealmlistWindow : public QDialog {
    Q_OBJECT

public:
    explicit RealmlistWindow(ConfigManager* config, QWidget* parent = nullptr);
    ~RealmlistWindow() override = default;

signals:
    void realmlistChanged();

private slots:
    void onAddServer();
    void onEditServer();
    void onDeleteServer();
    void onApplyChanges();
    void onServerSelected(QAbstractButton* button);

private:
    void setupUi();
    void applyStyle();
    void loadRealmlistEntries();
    QWidget* createServerWidget(const RealmlistEntry& entry, int index, bool isActive);
    void updateCurrentServerLabel();

    ConfigManager* m_config;
    int m_selectedIndex = -1;

    // UI Elements
    QWidget* m_serversContainer = nullptr;
    QVBoxLayout* m_serversLayout = nullptr;
    QButtonGroup* m_buttonGroup = nullptr;
    QLabel* m_currentServerLabel = nullptr;
    QPushButton* m_addButton = nullptr;
    QPushButton* m_editButton = nullptr;
    QPushButton* m_deleteButton = nullptr;
    QPushButton* m_applyButton = nullptr;
};

/**
 * @brief Dialog for adding/editing a server entry
 */
class ServerEditDialog : public QDialog {
    Q_OBJECT

public:
    explicit ServerEditDialog(QWidget* parent = nullptr,
                              const QString& name = "",
                              const QString& address = "");
    
    QString getName() const;
    QString getAddress() const;

private slots:
    void validate();

private:
    void setupUi(const QString& name, const QString& address);

    class QLineEdit* m_nameEdit = nullptr;
    class QLineEdit* m_addressEdit = nullptr;
};
