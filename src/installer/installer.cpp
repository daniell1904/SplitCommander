#include "installer.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    
    InstallerWindow window;
    window.show();
    
    return app.exec();
}
