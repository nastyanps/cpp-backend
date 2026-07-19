#pragma once
#include "author_fwd.h"
#include "book_fwd.h"

#include <memory>

namespace domain {

class UnitOfWork {
public:
    virtual ~UnitOfWork() = default;

    virtual void Commit() = 0;
    virtual AuthorRepository& Authors() = 0;
    virtual BookRepository& Books() = 0;
};

class UnitOfWorkFactory {
public:
    virtual std::unique_ptr<UnitOfWork> CreateUnitOfWork() = 0;

protected:
    ~UnitOfWorkFactory() = default;
};

}  // namespace domain
