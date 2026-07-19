#pragma once
#include <pqxx/connection>
#include <pqxx/transaction>
#include <memory>

#include "../domain/author.h"
#include "../domain/book.h"
#include "../domain/unit_of_work.h"

namespace postgres {

class AuthorRepositoryImpl : public domain::AuthorRepository {
public:
    explicit AuthorRepositoryImpl(pqxx::work& work)
        : work_{work} {
    }

    void Save(const domain::Author& author) override;
    std::vector<domain::Author> GetAuthors() override;
    std::optional<domain::Author> GetAuthorByName(const std::string& name) override;
    std::optional<domain::Author> GetAuthorById(const domain::AuthorId& id) override;
    void Delete(const domain::AuthorId& id) override;
    void Edit(const domain::AuthorId& id, const std::string& new_name) override;

private:
    pqxx::work& work_;
};

class BookRepositoryImpl : public domain::BookRepository {
public:
    explicit BookRepositoryImpl(pqxx::work& work)
        : work_{work} {
    }

    void Save(const domain::Book& book) override;
    std::vector<domain::Book> GetAllBooks() override;
    std::vector<domain::Book> GetAuthorBooks(const domain::AuthorId& author_id) override;
    std::vector<domain::BookInfo> GetAllBooksWithAuthors() override;
    std::optional<domain::Book> GetBookById(const domain::BookId& id) override;
    void Delete(const domain::BookId& id) override;
    void Edit(const domain::BookId& id, const std::string& new_title, int new_year,
              const std::vector<std::string>& new_tags) override;
    void DeleteByAuthor(const domain::AuthorId& author_id) override;

private:
    pqxx::work& work_;

    std::vector<std::string> GetTags(const domain::BookId& book_id);
    void SaveTags(const domain::BookId& book_id, const std::vector<std::string>& tags);
};

class UnitOfWorkImpl : public domain::UnitOfWork {
public:
    explicit UnitOfWorkImpl(pqxx::connection& connection)
        : work_{connection}
        , authors_{work_}
        , books_{work_} {
    }

    void Commit() override {
        work_.commit();
    }

    domain::AuthorRepository& Authors() override {
        return authors_;
    }

    domain::BookRepository& Books() override {
        return books_;
    }

private:
    pqxx::work work_;
    AuthorRepositoryImpl authors_;
    BookRepositoryImpl books_;
};

class UnitOfWorkFactoryImpl : public domain::UnitOfWorkFactory {
public:
    explicit UnitOfWorkFactoryImpl(pqxx::connection& connection)
        : connection_{connection} {
    }

    std::unique_ptr<domain::UnitOfWork> CreateUnitOfWork() override {
        return std::make_unique<UnitOfWorkImpl>(connection_);
    }

private:
    pqxx::connection& connection_;
};

class Database {
public:
    explicit Database(pqxx::connection connection);

    domain::UnitOfWorkFactory& GetUnitOfWorkFactory() & {
        return uow_factory_;
    }

private:
    pqxx::connection connection_;
    UnitOfWorkFactoryImpl uow_factory_{connection_};
};

}  // namespace postgres
