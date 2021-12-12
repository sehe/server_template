#include "src/logic/logic.hxx"
#include <iomanip>
#include <iostream>

void handleMessage(std::string const&                msg,
                   std::list<std::shared_ptr<User>>& users,
                   std::shared_ptr<User>             user)
{
    std::cout << "please implement handle message " << std::quoted(msg)
              << std::endl;
    for (auto& usr : users) {
        std::cout << " - " << usr->msgQueue.size() << std::endl;
    }
    user->msgQueue.emplace_back("please implement handle message");
}
