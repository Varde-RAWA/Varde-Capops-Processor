#include <QCoreApplication>
#include "app/ProcessorApp.hpp"

#include <exception>
#include <iostream>

int main(int argc, char *argv[])
{
    QCoreApplication qtApp(argc, argv);

    try
    {
        ProcessorApp app;
        app.run();
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << "Processor stopped: " << error.what() << '\n';
        return 1;
    }
}