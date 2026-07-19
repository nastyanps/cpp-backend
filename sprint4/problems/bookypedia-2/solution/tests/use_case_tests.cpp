#include <catch2/catch_test_macros.hpp>
#include "../src/app/use_cases_impl.h"
#include "../src/domain/author.h"
#include "../src/domain/book.h"
#include "../src/domain/unit_of_work.h"

namespace {

struct MockAuthorRepository : domain::AuthorRepository {
    std::vector<domain::Author> saved_authors;

    void Save(const domain::Author& author) override {
        saved_authors.emplace_back(author);
    }
    std::vector<domain::Author> GetAuthors() override {
        return saved_authors;
    }
    std::optional<domain::Author> GetAuthorByName(const std::string& name) override {
        for (const auto& a : saved_authors) {
            if (a.GetName() == name) {
                return a;
            }
        }
        return std::nullopt;
    }
    std::optional<domain::Author> GetAuthorById(const domain::AuthorId& id) override {
        for (const auto& a : saved_authors) {
            if (a.GetId() == id) {
                return a;
            }
        }
        return std::nullopt;
    }
    void Delete(const domain::AuthorId& id) override {
        std::erase_if(saved_authors, [&id](const domain::Author& a) { return a.GetId() == id; });
    }
    void Edit(const domain::AuthorId& id, const std::string& new_name) override {
        for (auto& a : saved_authors) {
            if (a.GetId() == id) {
                a = domain::Author{id, new_name};
                return;
            }
        }
        throw std::runtime_error("Author not found");
    }
};

struct MockBookRepository : domain::BookRepository {
    std::vector<domain::Book> saved_books;

    void Save(const domain::Book& book) override {
        saved_books.emplace_back(book);
    }
    std::vector<domain::Book> GetAllBooks() override {
        return saved_books;
    }
    std::vector<domain::Book> GetAuthorBooks(const domain::AuthorId& author_id) override {
        std::vector<domain::Book> result;
        for (const auto& book : saved_books) {
            if (book.GetAuthorId() == author_id) {
                result.push_back(book);
            }
        }
        return result;
    }
    std::vector<domain::BookInfo> GetAllBooksWithAuthors() override {
        return {};
    }
    std::optional<domain::Book> GetBookById(const domain::BookId& id) override {
        for (const auto& book : saved_books) {
            if (book.GetId() == id) {
                return book;
            }
        }
        return std::nullopt;
    }
    void Delete(const domain::BookId& id) override {
        std::erase_if(saved_books, [&id](const domain::Book& b) { return b.GetId() == id; });
    }
    void Edit(const domain::BookId& id, const std::string& new_title, int new_year,
              const std::vector<std::string>& new_tags) override {
        for (auto& book : saved_books) {
            if (book.GetId() == id) {
                book = domain::Book{id, book.GetAuthorId(), new_title, new_year, new_tags};
                return;
            }
        }
        throw std::runtime_error("Book not found");
    }
    void DeleteByAuthor(const domain::AuthorId& author_id) override {
        std::erase_if(saved_books,
                      [&author_id](const domain::Book& b) { return b.GetAuthorId() == author_id; });
    }
};

struct MockUnitOfWork : domain::UnitOfWork {
    MockUnitOfWork(MockAuthorRepository& authors, MockBookRepository& books)
        : authors_{authors}
        , books_{books} {
    }

    void Commit() override {
    }
    domain::AuthorRepository& Authors() override {
        return authors_;
    }
    domain::BookRepository& Books() override {
        return books_;
    }

private:
    MockAuthorRepository& authors_;
    MockBookRepository& books_;
};

struct MockUnitOfWorkFactory : domain::UnitOfWorkFactory {
    MockAuthorRepository authors;
    MockBookRepository books;

    std::unique_ptr<domain::UnitOfWork> CreateUnitOfWork() override {
        return std::make_unique<MockUnitOfWork>(authors, books);
    }
};

struct Fixture {
    MockUnitOfWorkFactory uow_factory;
};

}  // namespace

SCENARIO_METHOD(Fixture, "Book Adding") {
    GIVEN("Use cases") {
        app::UseCasesImpl use_cases{uow_factory};
        WHEN("Adding an author") {
            const auto author_name = "Joanne Rowling";
            use_cases.AddAuthor(author_name);
            THEN("author with the specified name is saved to repository") {
                REQUIRE(uow_factory.authors.saved_authors.size() == 1);
                CHECK(uow_factory.authors.saved_authors.at(0).GetName() == author_name);
                CHECK(uow_factory.authors.saved_authors.at(0).GetId() != domain::AuthorId{});
            }
        }
    }
}
