<?php
/**
 * Provide an extended class for holding multiple ChadoRecords for insertion or 
 * manipulation. It is intended to provide bulk updates where applicable.
 *
 * Specifically ths class provides select, insert, update and delete.
 *
 * Run tests can be done using devel/php on Tripal sites example:
 * include_once(DRUPAL_ROOT . '/sites/all/modules/tripal/tests/tripal_chado/api/
 * ChadoRecordListTest.php');
 * $test = new Tests\ChadoRecordListTest();
 * dpm($test->test_chadorecordlist_insert_multiple(10));
 * 
 */

class ChadoRecordList {
    /**
     * chado_records holds ChadoRecord objects.
     *
     * @var array
     */   
    protected $chado_records = [];

    /**
     * chado_records_count holds the current count of chado_records for 
     * optimizations.
     *
     * @var int
     */   
    protected $chado_records_count = -1;    

    /**
     * table_name stores the database table's name for which chado records must 
     * conform to. 
     *
     * @var string
     */   
    protected $table_name = '';
      
    /**
     * schema holds the schema definition for the table
     *
     * @var string
     */   
    protected $schema = '';
   

    /**
     * action_perform keeps track of whether actions have already been performed
     * on this chadorecordlist 
     *
     * @var int
     */   
    protected $action_performed = FALSE;

    /**
     * last action keeps track of the last action performed 
     * on this chadorecordlist 
     *
     * @var String or null
     */   
    protected $action_last = null;

    /**
     * A general-purpose constructor to initialize ChadoRecordList.
     *
     * @param string $table_name
     *    A string that contains the name of the table in which this 
     *    ChadoRecordList administers
     * 
     * @param int $action_limit
     *    An int which contains the action_limit / partition limit when
     *    performing bulk actions
     *
     * @throws Exception
     */
    public function __construct($table_name, $action_limit = 1000) {
        // Retrieve schema using the chado_get_schema API function
        $this->schema = chado_get_schema($table_name);

        if (!$this->schema) {
            $message = t('ChadoRecordList::__construct(). Could not find a ' .
                       'matching table schema in Chado for the table: !table.',
              ['!table' => $this->table_name]);
            throw new Exception($message);
        }

        // Only happens if a schema was found
        $this->setTableName($table_name);
        $this->setActionLimit($action_limit);        
       
    }

    /**
     * setValues iterates through a 2D array calling the addValues function 
     * per array item in $values
     *
     * @param array $values
     *    A 2D array contains 'rows' of 'associative arrays'. The first 
     *    dimension can be considered row. The second dimension contains 
     *    associated array format columns and values.
     * 
     * @throws Exception
     */
    public function setValues($values) {
        if (!$this->isActionPerformed()) {
            $records_count = 0;
            foreach ($values as $row) {
                $this->addValues($row);
                $records_count = $records_count + 1;
            }
            $this->setChadoRecordsCount($records_count);
            return $records_count;
        }
        else {
            $message = t('ChadoRecordList::setValues(). Unable to perform ' .
                       'setValues since actions have already been performed.');
            throw new Exception($message);
        }
    }
    
    /**
     * addValues creates a new ChadoRecord object
     *
     * @param array $values
     *    An associated array containing column names and values.
     * 
     * @throws Exception
     */
    public function addValues($values) {
        // Check if actions have not been performed on this ChadoRecordList, 
        // else throw error
        if (!$this->action_performed) {
            if ($this->table_name != '' && $this->table_name != null) {
                // Initialize a ChadoRecord object with specified table
                $record = new ChadoRecord($this->table_name);

                // Check if values are not null, else throw error
                if ($values != null) {
                    $record->setValues($values);
                }
                else {
                    $message = t('ChadoRecordList::addValues(). Values must ' 
                               . 'not be null.');
                    throw new Exception($message);
                }

                // Add the record to chado_records
                $this->chado_records[] = $record;
            }
            else {
                $message = t('ChadoRecordList::addValues(). Table name must ' . 
                           'be set to something other than empty or null.');
                throw new Exception($message);
            }
        }
        else {
            $message = t('ChadoRecordList::addValues(). Unable to perform ' .
                       'addValues since actions have already been performed.');
            throw new Exception($message);
        }
    }   
    
    /**
     * insert performs a bulk database insert
     *
     * @param boolean $return_keys
     *    Sets whether to return the keys or not
     * 
     * @return array $inserted_pkeys_id
     *      Returns an array of primary keys of the inserted rows
     * 
     * @throws Exception
     */ 
    public function insert() {
        // Get table schema since we may need it later on in the function.
        $table_schema = $this->getSchema();

        // Check if actions have not been performed on this ChadoRecordList, 
        // else throw error.
        if (!$this->isActionPerformed()) {
            $records_count = $this->getChadoRecordsCount();

            // Check if records exist using records_count before performing
            // actual inserts.
            if ($records_count < 1) {
                $message = t('ChadoRecordList::insert(). Could not insert a ' .
                            'record into the table !table, no records found.',
                ['!table' => $this->getTableName()]);
                throw new Exception($message);
            }  


            // Get the primary key for this table
            $primary_key_name = $this->getSchema()['primary key'][0];

            // This code is a mixture of Stephen's GFF example plus extra 
            // column variations
            $sql_init = 'INSERT INTO {' . $this->getTableName() . '} (';

            // Get all fields/column names from table schema, remove the 
            // primary key
            foreach ($this->getSchema()['fields'] as $column_name => $column_properties) {
                if ($column_name != $primary_key_name) {
                    $sql_init .= $column_name . ',';
                }
            }

            // remove trailing comma
            $sql_init = rtrim($sql_init, ',');
            $sql_init .= ") VALUES \n";


            $sql_return = ' RETURNING ' . $primary_key_name . ' AS pkey';

            
            $result = NULL; 
            $sql = ''; 
            // Array that stores the placeholders and values to be inserted
            $placeholders = array(); 
            //Array store the primary keys generated from inserting the records
            $inserted_pkey_ids = array();  
            
            // Iterate through each record
            for ($i = 0; $i < $records_count; $i++) { 
                // Get the ith record
                $record = $this->chado_records[$i];
                
                // Check if record does not contain a primary key, we can do an 
                // insert
                if ($record->getRecordId() == NULL) {
                    // Get field values for this specific record, 
                    // we need to cater for those that are missing
                    $values = $record->getValues();
                    $sql .= '(';

                    // @TODO Check if all required fields are set before continuing?
                    // Seems to be not required for organism, test on other table
                    foreach ($this->getSchema()['fields'] as $column_name => $column_properties) {
                        if ($column_name != $primary_key_name) {
                            $placeholder_text = ':' . $column_name . '_' . $i;
                            $sql .= $placeholder_text . ',';
                            //@ to ignore if empty
                            $placeholders[$placeholder_text] = @$values[$column_name]; 
                        }
                    }
                    $sql = rtrim($sql, ',');//remove trailing ,
                    $sql .=  "),\n";
                }
            }      
            $sql = rtrim($sql, ",\n");
            // build sql + force returning of pkey ids
            $sql = $sql_init . $sql . $sql_return;  
            // dpm($sql);  
            $result = chado_query($sql, $placeholders);
            foreach ($result as $row) {
                $inserted_pkey_ids[] = $row->pkey;
            }
        
               
            
            // Add the primary keys back into the chado records
            for ($i=0; $i < $this->getChadoRecordsCount(); $i++) {
                // Set the RecordId using the inserted pkey id for this
                // Chado Record
                $this->chado_records[$i]->setRecordId($inserted_pkey_ids[$i]);
            }

            // Set action_performed for this ChadoListRecord as true to 
            // preserve integrity
            $this->action_performed = TRUE;
            $this->action_last = 'insert';

            // Return keys if $return_keys = TRUE
            return $inserted_pkey_ids;
        }
        else {
            $message = t('ChadoRecordList::insert(). Unable to perform insert '.
                        'since actions have already been performed on this ' .
                        'ChadoRecordList.');
            throw new Exception($message);           
        }
    }

    /**
     * Deletes the record that matches the given values.
     *
     * Record IDs must exist for this function to successfully execute.
     *
     * @throws Exception
     */
    public function delete() {    
        // Go through each chado record checking to make sure a record_id
        // (primary key) is present 
        $sql = '';
        $sql_init = 'DELETE FROM {' . 
            $this->getTableName() . '} WHERE ' . 
            $this->getSchema()['primary key'][0] . ' IN (';
        $sql_end = ')'; 
        $ids = '';
        for ($i=0; $i < $this->getChadoRecordsCount(); $i++) {
            $record_id = $this->chado_records[$i]->getRecordId();
            if($record_id != NULL) {
                //dpm('Adding id:' . $record_id);
                $ids .= $record_id . ',';
            }
            else {
                $message = t('ChadoRecordList::delete(). Could not perform 
                delete on !table because item !i in chado_records does not have 
                a valid primary key',
                ['!table' => $this->getTableName(),'!i' => $i]);
                throw new Exception($message);                
            }
        }
        $ids = rtrim($ids, ',');
        $sql = $sql_init . $ids . $sql_end;

        $result = chado_query($sql, array());
        $this->action_last = 'delete';
        return $result;
    }

    /**
     * get schema from protected variable
     *
     * @param array $this->schema
     *    An associated array containing the tables schema and column information.
     * 
     */  
    function getSchema() {
        return $this->schema;
    }

    /**
     * get action limit from protected variable
     *
     * @param int $this->action_limit
     *    An integer value representing the action_limit / bulk interval
     * 
     */  
    function getActionLimit() {
        return $this->action_limit;
    }  
    
    /**
     * set action limit into protected variable
     */  
    function setActionLimit($action_limit) {
        $this->action_limit = $action_limit;
    }     
    
    /**
     * get table name from protected variable
     * 
     * @return string table name
     */  
    function getTableName() {
        return $this->table_name;
    }  

    /**
     * set table name from protected variable
     * 
     * @param string $table_name
     *    A string values representing the table name
     * 
     */  
    function setTableName($table_name) {
        $this->table_name = $table_name;
    }      
    
    /**
     * get action performed from protected variable
     *
     * @return boolean $this->action_performed
     *    A boolean value representing the state of action performed
     * 
     */  
    function isActionPerformed() {
        return $this->action_performed;
    } 
    
    /**
     * get chado records from protected variable
     *
     * @return array $this->chado_records
     *    An array that contains ChadoRecords
     * 
     */  
    function getChadoRecords() {
        return $this->chado_records;
    }  
    
    /**
     * get chado records count from protected variable
     *
     * @return int $this->chado_records_count
     *    An integer that contains the current chado records count
     * 
     */  
    function getChadoRecordsCount() {
        if ($this->chado_records_count == -1) {
            $this->setChadoRecordsCount(count($this->chado_records));
        }
        return $this->chado_records_count;
    } 
    
    /**
     * set chado records count from protected variable (helper function for
     * speed optimization)
     * 
     * @param int $count
     * Sets the current Chado Records count 
     */  
    function setChadoRecordsCount($count) {
        $this->chado_records_count = $count;
    }
}

?>