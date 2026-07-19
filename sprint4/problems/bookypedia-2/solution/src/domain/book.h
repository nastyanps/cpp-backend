#pragma once
#include <optional>
#include <string>
#include <vector>

#include "author.h"
#include "../util/tagged_uuid.h"

namespace domain {

namespace detail {
struct BookTag {};
}  // namespace detail

using BookId = util::TaggedUUID<detail::BookTag>;

class Book {
public:
    Book(BookId id, AuthorId author_id, std::string title, int publication_year,
         std::vector<std::string> tags = {})
        : id_(std::move(id))
        , author_id_(std::move(author_id))
        , title_(std::move(title))
        , publication_year_(publication_year)
        , tags_(std::move(tags)) {
    }

    const BookId& GetId() const noexcept {
        return id_;
    }

    const AuthorId& GetAuthorId() const noexcept {
        return author_id_;
    }

    const std::string& GetTitle() const noexcept {
        return title_;
    }

    int GetPublicationYear() const noexcept {
        return publication_year_;
    }

    const std::vector<std::string>& GetTags() const noexcept {
        return tags_;
    }

private:
    BookId id_;
    AuthorId author_id_;
    std::string title_;
    int publication_year_;
    std::vector<std::string> tags_;
};

struct BookInfo {
    BookId id;
    AuthorId author_id;
    std::string author_name;
    std::string title;
    int publication_year;
};

class BookRepository {
public:
    virtual void Save(const Book& book) = 0;
    virtual std::vector<Book> GetAllBooks() = 0;
    virtual std::vector<Book> GetAuthorBooks(const AuthorId& author_id) = 0;
    virtual std::vector<BookInfo> GetAllBooksWithAuthors() = 0;
    virtual std::optional<Book> GetBookById(const BookId& id) = 0;
    virtual void Delete(const BookId& id) = 0;
    virtual void Edit(const BookId& id, const std::string& new_title, int new_year,
                       const std::vector<std::string>& new_tags) = 0;
    virtual void DeleteByAuthor(const AuthorId& author_id) = 0;

protected:
    ~BookRepository() = default;
};

}  // namespace domain
