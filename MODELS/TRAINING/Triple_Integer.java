
public class Triple_Integer {
    private final Integer subject;
    private final Integer predicate;
    private final Integer object;

    public Triple_Integer(Integer subject, Integer predicate, Integer object) {
        this.subject = subject;
        this.predicate = predicate;
        this.object = object;
    }

    public Integer getSubject() {
        return subject;
    }

    public Integer getPredicate() {
        return predicate;
    }

    public Integer getObject() {
        return object;
    }
}

