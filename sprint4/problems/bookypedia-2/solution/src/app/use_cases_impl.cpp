#include "use_cases_impl.h"

#include <algorithm>
#include <set>
#include <stdexcept>

#include "../domain/author.h"
#include "../domain/book.h"

namespace app {
using namespace domain;

namespace {

std::string NormalizeSpaces(const std::string& s) {
    std::string result;
    bool prev_space = true;
    for (char c : s) {
        if (std::isspace(static_cast<unsigned char>(c))) {
            if (!prev_space) {
                result += ' ';
            }
            prev_space = true;
        } else {
            result += c;
            prev_space = false;
        }
    }
    while (!result.empty() && result.back() == ' ') {
        result.pop_back();
    }
    return result;
}

}  // namespace

std::vector<std::string> UseCasesImpl::NormalizeTags(const std::vector<std::string>& raw_tags) {
    std::set<std::string> unique_tags;
    for (const auto& raw : raw_tags) {
        std::string normalized = NormalizeSpaces(raw);
        if (!normalized.empty()) {
            unique_tags.insert(normalized);
        }
    }
    return {unique_tags.begin(), unique_tags.end()};
}

void UseCasesImpl::AddAuthor(const std::string& name) {
    if (name.empty()) {
        throw std::invalid_argument("Author name is empty");
    }
    auto uow = uow_factory_.CreateUnitOfWork();
    uow->Authors().Save({AuthorId::New(), name});
    uow->Commit();
}

std::vector<AuthorInfo> UseCasesImpl::GetAuthors() {
    auto uow = uow_factory_.CreateUnitOfWork();
    std::vector<AuthorInfo> result;
    for (const auto& author : uow->Authors().GetAuthors()) {
        result.push_back({author.GetId().ToString(), author.GetName()});
    }
    return result;
}

std::optional<AuthorInfo> UseCasesImpl::GetAuthorByName(const std::string& name) {
    auto uow = uow_factory_.CreateUnitOfWork();
    auto author = uow->Authors().GetAuthorByName(name);
    if (!author) {
        return std::nullopt;
    }
    return AuthorInfo{author->GetId().ToString(), author->GetName()};
}

void UseCasesImpl::AddBook(const std::string& author_id, const std::string& title,
                           int publication_year, const std::vector<std::string>& tags) {
    auto uow = uow_factory_.CreateUnitOfWork();
    uow->Books().Save({BookId::New(), AuthorId::FromString(author_id), title, publication_year,
                       NormalizeTags(tags)});
    uow->Commit();
}

std::vector<BookInfo> UseCasesImpl::GetBooks() {
    auto uow = uow_factory_.CreateUnitOfWork();
    std::vector<BookInfo> result;
    for (const auto& info : uow->Books().GetAllBooksWithAuthors()) {
        result.push_back({info.id.ToString(), info.author_id.ToString(), info.author_name,
                          info.title, info.publication_year});
    }
    return result;
}

std::vector<BookInfo> UseCasesImpl::GetAuthorBooks(const std::string& author_id) {
    auto uow = uow_factory_.CreateUnitOfWork();
    AuthorId aid = AuthorId::FromString(author_id);
    auto author = uow->Authors().GetAuthorById(aid);
    std::string author_name = author ? author->GetName() : "";

    std::vector<BookInfo> result;
    for (const auto& book : uow->Books().GetAuthorBooks(aid)) {
        result.push_back({book.GetId().ToString(), author_id, author_name, book.GetTitle(),
                          book.GetPublicationYear()});
    }
    return result;
}

std::vector<BookInfo> UseCasesImpl::GetBooksByTitle(const std::string& title) {
    auto uow = uow_factory_.CreateUnitOfWork();
    std::vector<BookInfo> result;
    for (const auto& info : uow->Books().GetAllBooksWithAuthors()) {
        if (info.title == title) {
            result.push_back({info.id.ToString(), info.author_id.ToString(), info.author_name,
                              info.title, info.publication_year});
        }
    }
    return result;
}

std::optional<BookDetails> UseCasesImpl::GetBookDetails(const std::string& book_id) {
    auto uow = uow_factory_.CreateUnitOfWork();
    BookId bid = BookId::FromString(book_id);
    auto book = uow->Books().GetBookById(bid);
    if (!book) {
        return std::nullopt;
    }
    auto author = uow->Authors().GetAuthorById(book->GetAuthorId());
    std::string author_name = author ? author->GetName() : "";

    return BookDetails{book->GetTitle(), author_name, book->GetPublicationYear(),
                       book->GetTags()};
}

bool UseCasesImpl::DeleteAuthor(const std::string& author_id) {
    try {
        auto uow = uow_factory_.CreateUnitOfWork();
        AuthorId aid = AuthorId::FromString(author_id);
        if (!uow->Authors().GetAuthorById(aid)) {
            return false;
        }
        uow->Books().DeleteByAuthor(aid);
        uow->Authors().Delete(aid);
        uow->Commit();
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

bool UseCasesImpl::DeleteAuthorByName(const std::string& name) {
    try {
        auto uow = uow_factory_.CreateUnitOfWork();
        auto author = uow->Authors().GetAuthorByName(name);
        if (!author) {
            return false;
        }
        uow->Books().DeleteByAuthor(author->GetId());
        uow->Authors().Delete(author->GetId());
        uow->Commit();
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

bool UseCasesImpl::EditAuthor(const std::string& author_id, const std::string& new_name) {
    try {
        auto uow = uow_factory_.CreateUnitOfWork();
        AuthorId aid = AuthorId::FromString(author_id);
        if (!uow->Authors().GetAuthorById(aid)) {
            return false;
        }
        uow->Authors().Edit(aid, new_name);
        uow->Commit();
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

bool UseCasesImpl::DeleteBook(const std::string& book_id) {
    try {
        auto uow = uow_factory_.CreateUnitOfWork();
        BookId bid = BookId::FromString(book_id);
        if (!uow->Books().GetBookById(bid)) {
            return false;
        }
        uow->Books().Delete(bid);
        uow->Commit();
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

bool UseCasesImpl::EditBook(const std::string& book_id, const std::string& new_title,
                            int new_year, const std::vector<std::string>& new_tags) {
    try {
        auto uow = uow_factory_.CreateUnitOfWork();
        BookId bid = BookId::FromString(book_id);
        if (!uow->Books().GetBookById(bid)) {
            return false;
        }
        uow->Books().Edit(bid, new_title, new_year, NormalizeTags(new_tags));
        uow->Commit();
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

}  // namespace app
